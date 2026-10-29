#include "WVoxChainDsp.h"
#include "WVoxChainIR.h"
#include <thread>

using namespace juce;

namespace wvoxchain::dsp
{
namespace
{
// the reverb's responses for one rate (nulltest/pack_voxchain_reverb.py): per colour a length, then LL, LR, RL, RR
// interleaved as 16-bit floats
struct Packed { double fs; const char* data; int size; };

float half (uint16 h)
{
    const uint32 s = (uint32) (h & 0x8000u) << 16, e = (h >> 10) & 0x1fu, m = h & 0x3ffu;
    uint32 f;
    if (e == 0) { if (m == 0) f = s; else { int ee = -1; uint32 mm = m; do { ++ee; mm <<= 1; } while ((mm & 0x400u) == 0); f = s | ((uint32) (127 - 15 - ee) << 23) | ((mm & 0x3ffu) << 13); } }
    else if (e == 31) f = s | 0x7f800000u | (m << 13);
    else f = s | ((e + 112u) << 23) | (m << 13);
    float v;
    std::memcpy (&v, &f, 4);
    return v;
}

Packed nearestPacked (double fs)
{
    const Packed all[] = { { 44100.0, WVoxChainIR::reverb_44100_bin, WVoxChainIR::reverb_44100_binSize },
                           { 48000.0, WVoxChainIR::reverb_48000_bin, WVoxChainIR::reverb_48000_binSize },
                           { 88200.0, WVoxChainIR::reverb_88200_bin, WVoxChainIR::reverb_88200_binSize },
                           { 96000.0, WVoxChainIR::reverb_96000_bin, WVoxChainIR::reverb_96000_binSize } };
    const Packed* best = &all[0];
    for (const auto& p : all) if (std::abs (std::log (p.fs / fs)) < std::abs (std::log (best->fs / fs))) best = &p;
    return *best;
}
}

//==============================================================================
void Reverb::prepare (double fs, int block)
{
    const auto pk = nearestPacked (fs);
    const auto* p = reinterpret_cast<const uint8*> (pk.data);
    const juce::dsp::ProcessSpec spec { fs, (uint32) jmax (1, block), 2 };
    for (int c = 0; c < 3; ++c)
    {
        uint32 n;
        std::memcpy (&n, p, 4); p += 4;
        AudioBuffer<float> direct (2, (int) n), cross (2, (int) n);       // (LL, RR) and (RL, LR): the outputs from each input
        for (uint32 i = 0; i < n; ++i)
        {
            uint16 v[4];
            std::memcpy (v, p + (size_t) i * 8, 8);
            direct.setSample (0, (int) i, half (v[0])); direct.setSample (1, (int) i, half (v[3]));
            cross.setSample (0, (int) i, half (v[2])); cross.setSample (1, (int) i, half (v[1]));
        }
        p += (size_t) n * 8;
        for (int k = 0; k < 2; ++k)
        {
            auto& e = conv[(size_t) c][(size_t) k];
            e = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::NonUniform { 256 });
            e->prepare (spec);
            e->loadImpulseResponse (k == 0 ? std::move (direct) : std::move (cross), pk.fs, juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        }
    }
    // the responses load on a background thread and are swapped in during processing (with a short crossfade): run
    // silence through every engine until each holds its response, so the first real block already has it
    a.setSize (2, jmax (1, block)); b.setSize (2, jmax (1, block));
    const auto t0 = Time::getMillisecondCounter();
    for (;;)
    {
        bool ready = true;
        for (auto& c : conv)
            for (auto& e : c)
            {
                a.clear();
                juce::dsp::AudioBlock<float> blk (a);
                e->process (juce::dsp::ProcessContextReplacing<float> (blk));
                ready = ready && e->getCurrentIRSize() > 0;
            }
        if (ready || Time::getMillisecondCounter() - t0 > 5000) break;
        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
    for (int i = 0; i < (int) (0.2 * fs) / jmax (1, block) + 2; ++i)    // (past the crossfade)
        for (auto& c : conv)
            for (auto& e : c) { a.clear(); juce::dsp::AudioBlock<float> blk (a); e->process (juce::dsp::ProcessContextReplacing<float> (blk)); }
    reset();
    running = 0;
}

void Reverb::process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain)
{
    if (colour > 0 && colour != running) { running = colour; for (auto& e : conv[(size_t) running - 1]) e->reset(); }
    if (running == 0) { FloatVectorOperations::clear (outL, n); FloatVectorOperations::clear (outR, n); return; }
    a.setSize (2, n, false, false, true); b.setSize (2, n, false, false, true);
    a.copyFrom (0, 0, inL, n); a.copyFrom (1, 0, inR, n);
    b.copyFrom (0, 0, inR, n); b.copyFrom (1, 0, inL, n);
    {
        juce::dsp::AudioBlock<float> ba (a), bb (b);
        conv[(size_t) running - 1][0]->process (juce::dsp::ProcessContextReplacing<float> (ba));
        conv[(size_t) running - 1][1]->process (juce::dsp::ProcessContextReplacing<float> (bb));
    }
    const float g = colour > 0 ? gain : 0.0f;                          // (clear: muted, the engine still running)
    for (int i = 0; i < n; ++i)
    {
        outL[i] = g * (a.getSample (0, i) + b.getSample (0, i));
        outR[i] = g * (a.getSample (1, i) + b.getSample (1, i));
    }
}

//==============================================================================
void Chain::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    const int k = fs > 144000.0 ? 4 : fs > 72000.0 ? 2 : 1;
    latency = 193 * k - (k - 1);
    lineL.assign ((size_t) latency + 1, 0.0f); lineR.assign ((size_t) latency + 1, 0.0f);
    pos = 0;
    // the fixed filter at the nearest fitted rate
    const fixed::Rate* best = &fixed::rates[0];
    for (const auto& r : fixed::rates) if (std::abs (std::log (r.fs / fs)) < std::abs (std::log (best->fs / fs))) best = &r;
    for (int i = 0; i < 4; ++i) { fixedFilter[(size_t) i].set (best->sos[i]); fixedFilter[(size_t) i].reset(); }
    bass.prepare (fs, true); treble.prepare (fs, false);
    reverb.prepare (fs, maxBlock);
    tap.setSize (2, jmax (1, maxBlock)); wet.setSize (2, jmax (1, maxBlock));
    levelCoef = std::exp (-1.0 / (0.010 * fs));
    level = 0.0;
    // the Noise (on in the reference by default): white, uncorrelated L / R, -108.3 dBFS rms at the output with every
    // colour clear (the fixed filter adds about +0.4 dB to white noise)
    noiseGain = std::pow (10.0, -108.7 / 20.0);
}

void Chain::reset()
{
    std::fill (lineL.begin(), lineL.end(), 0.0f); std::fill (lineR.begin(), lineR.end(), 0.0f);
    for (auto& f : fixedFilter) f.reset();
    reverb.reset();
}

void Chain::process (float* l, float* r, int n, const Params& p)
{
    const double gIn = std::pow (10.0, p.sensitivity / 20.0), gOut = std::pow (10.0, p.output / 20.0);
    const double balL = std::pow (10.0, std::min (0.0f, -p.balance) / 20.0), balR = std::pow (10.0, std::min (0.0f, p.balance) / 20.0);
    bass.set (p.colour[0], p.fader[0]);
    treble.set (p.colour[1], p.fader[1]);
    if (tap.getNumSamples() < n) { tap.setSize (2, n, false, false, true); wet.setSize (2, n, false, false, true); }
    std::normal_distribution<double> gauss (0.0, 1.0);
    const int len = latency + 1;
    for (int i = 0; i < n; ++i)
    {
        double xl = l[i] * gIn, xr = r[i] * gIn;
        level = (0.5 * (xl * xl + xr * xr)) + levelCoef * (level - 0.5 * (xl * xl + xr * xr));
        xl *= balL; xr *= balR;
        if (noiseOn) { xl += noiseGain * gauss (noise); xr += noiseGain * gauss (noise); }
        lineL[(size_t) pos] = (float) xl; lineR[(size_t) pos] = (float) xr;
        pos = (pos + 1) % len;
        xl = lineL[(size_t) pos]; xr = lineR[(size_t) pos];
        bass.run (xl, xr);
        treble.run (xl, xr);
        // (Compress and the de-esser: their linear part only, for now)
        tap.setSample (0, i, (float) xl); tap.setSample (1, i, (float) xr);
        for (auto& f : fixedFilter) { xl = f.run (xl, 0); xr = f.run (xr, 1); }
        l[i] = (float) xl; r[i] = (float) xr;
    }
    // the wet stages on the tap: the reverb (1.2 dB per fader step from its measured response at 0)
    reverb.process (tap.getReadPointer (0), tap.getReadPointer (1), wet.getWritePointer (0), wet.getWritePointer (1), n,
                    p.colour[3], (float) std::pow (10.0, 1.2 * p.fader[3] / 20.0));
    for (int i = 0; i < n; ++i)
    {
        l[i] = (float) ((l[i] + wet.getSample (0, i)) * gOut);
        r[i] = (float) ((r[i] + wet.getSample (1, i)) * gOut);
    }
}
}
