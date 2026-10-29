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
void Compressor::prepare (double sampleRate)
{
    fs = sampleRate;
    la = jmax (1, (int) std::lround (64.0 * fs / 48000.0));
    bl.assign ((size_t) la + 1, 0.0f); br.assign ((size_t) la + 1, 0.0f);
    need.assign ((size_t) la + 1, 1.0); dq.assign ((size_t) la + 1, 0);
    active = -1;
    reset();
}

void Compressor::reset()
{
    gf = gs = pw = 0.0; lg = 1.0; w = 0; qh = qt = 0; count = 0;
    std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f); std::fill (need.begin(), need.end(), 1.0);
    for (auto& b : linear) b.reset();
}

void Compressor::process (float* l, float* r, int n, int colour, float fader)
{
    const int len = la + 1;
    if (colour != active)
    {
        active = colour;
        reset();
        if (colour > 0)
        {
            const auto& c = *comptables::colours[colour];
            const comptables::Linear* best = &c.linear[0];
            for (const auto& x : c.linear) if (std::abs (std::log (x.fs / fs)) < std::abs (std::log (best->fs / fs))) best = &x;
            for (int k = 0; k < 4; ++k) linear[(size_t) k].set (best->sos[k]);
        }
    }
    if (colour == 0)                                                  // clear: the look-ahead's delay only
    {
        for (int i = 0; i < n; ++i)
        {
            bl[(size_t) w] = l[i]; br[(size_t) w] = r[i];
            w = (w + 1) % len;
            l[i] = bl[(size_t) w]; r[i] = br[(size_t) w];
        }
        return;
    }
    // the parameters at the fader, linear between the fitted steps
    const auto& c = *comptables::colours[colour];
    const double x = juce::jlimit (0.0, 20.0, (double) fader + 10.0);
    const int k0 = juce::jmin (19, (int) x);
    const double t = x - k0;
    double p[comptables::nParams];
    for (int k = 0; k < comptables::nParams; ++k) p[k] = c.p[k0][k] + (c.p[k0 + 1][k] - c.p[k0][k]) * t;
    const double gin = std::pow (10.0, p[0] / 20.0), gout = std::pow (10.0, p[11] / 20.0);
    const double T = p[1], R = juce::jmax (1.0, p[2]), K = p[3];
    auto coef = [this] (double ms) { return ms <= 0.0 ? 0.0 : std::exp (-1.0 / (ms * 0.001 * fs)); };
    const double ca = coef (p[4]), cr = coef (p[5]), cs = coef (p[6]), share = p[7];
    const double ceil = std::pow (10.0, p[8] / 20.0), clr = coef (p[9]), cw = coef (p[13]), lk = p[14];
    const double a2 = p[15], bb = p[16], a3 = p[17];
    const bool after = p[18] >= 0.5, power = p[12] > 0.5;
    auto shape = [a2, bb, a3] (double u) { return u + a2 * u * u + bb * u * std::abs (u) + a3 * u * u * u; };
    for (int i = 0; i < n; ++i)
    {
        double a = l[i] * gin, b = r[i] * gin;
        if (! after) { a = shape (a); b = shape (b); }
        const double v = juce::jmax (std::abs (a), std::abs (b));
        double L;
        if (power) { pw = cw * pw + (1.0 - cw) * v * v; L = 10.0 * std::log10 (pw + 1.0e-30); }
        else L = 20.0 * std::log10 (v + 1.0e-30);
        const double over = L - T;
        double tr;
        if (K > 0.0 && over > -K / 2.0 && over < K / 2.0) tr = (1.0 - 1.0 / R) * (over + K / 2.0) * (over + K / 2.0) / (2.0 * K);
        else tr = over > 0.0 ? (1.0 - 1.0 / R) * over : 0.0;
        gf = tr > gf ? ca * gf + (1.0 - ca) * tr : cr * gf + (1.0 - cr) * tr;
        gs = tr > gs ? ca * gs + (1.0 - ca) * tr : cs * gs + (1.0 - cs) * tr;
        const double g = std::pow (10.0, -((1.0 - share) * gf + share * gs) / 20.0);
        a *= g; b *= g;
        if (after) { a = shape (a); b = shape (b); }
        // the limiter: the gain each sample needs, the lowest over the look-ahead at once, released slowly
        const double pk = juce::jmax (std::abs (a), std::abs (b));
        double nd = 1.0;
        if (pk > 1.0e-30)
        {
            const double o2 = 20.0 * std::log10 (pk / ceil);
            const double r2 = lk > 0.0 && o2 > -lk / 2.0 && o2 < lk / 2.0 ? (o2 + lk / 2.0) * (o2 + lk / 2.0) / (2.0 * lk) : juce::jmax (0.0, o2);
            nd = std::pow (10.0, -r2 / 20.0);
        }
        bl[(size_t) w] = (float) a; br[(size_t) w] = (float) b; need[(size_t) w] = nd;
        // (the window's minimum by a monotonic queue over the ring's slots, in sample order)
        const long long idx = count++;
        while (qt > qh && need[(size_t) (dq[(size_t) ((qt - 1) % len)] % len)] >= nd) --qt;
        dq[(size_t) (qt % len)] = (int) (idx % len); ++qt;
        // drop entries older than the window (their slot's sample is more than la back)
        while (qt - qh > len) ++qh;
        double mn = need[(size_t) dq[(size_t) (qh % len)]];
        if (qt - qh == len) { ++qh; mn = need[(size_t) dq[(size_t) (qh % len)]]; }
        lg = mn < lg ? mn : clr * lg + (1.0 - clr) * mn;
        const int rd = (w + 1) % len;
        double ol = bl[(size_t) rd] * lg * gout, orr = br[(size_t) rd] * lg * gout;
        for (auto& f : linear) { ol = f.run (ol, 0); orr = f.run (orr, 1); }
        l[i] = (float) ol; r[i] = (float) orr;
        w = rd;
    }
}

//==============================================================================
// the two taps' gains (calibrated on the reference's Slap: -16.1 dB at fader 0, the send's gain stage, SuperTap's tap
// level and its pan law together) and their delay (one sample past the line's)
static constexpr float kTap1 = 0.1566f, kTap2 = 0.1566f, kTap2Cross = 0.995f;
static constexpr int kTapDelay = 1;

void Delays::prepare (double sampleRate, int block)
{
    fs = sampleRate;
    analog.modes = wdelay::tables::analog;
    analog.prepare (fs, 2);
    analog.setMode (4);
    line.prepare (fs);
    work.setSize (2, juce::jmax (1, block));
    histL.assign ((size_t) (fs * 0.25) + 1, 0.0f); histR.assign (histL.size(), 0.0f);
    hpos = 0;
    running = 1;
}

void Delays::process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain, double bpm)
{
    if (colour > 0) running = colour;
    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);
    work.copyFrom (0, 0, inL, n); work.copyFrom (1, 0, inR, n);
    float* const ch[2] = { work.getWritePointer (0), work.getWritePointer (1) };
    analog.process (ch, n);
    static constexpr double beats[4] = { 0.0, 0.25, 0.5, 1.0 };
    static constexpr float fb[4] = { 0.0f, 0.0f, 0.5f, 0.3f }, outDb[4] = { 0.0f, -12.0f, -9.0f, -9.0f };
    const double tempo = bpm > 0.0 ? bpm : 120.0;
    wdelay::dsp::Settings st;
    st.timeL = st.timeR = juce::jlimit (1.0, 3500.0, beats[running] * 60000.0 / tempo) * fs / 1000.0;
    st.feedback = fb[running];
    st.mix = 1.0f;
    st.outputDb = outDb[running];
    st.hipassHz = 150.0f; st.lopassHz = 5000.0f;
    st.mode = 0;
    line.process (ch, 2, n, st);
    const int len = (int) histL.size();
    const int t2 = juce::jlimit (1, len - 1, (int) std::lround (0.004 * 4.0 * 60.0 / tempo * fs));
    const float g = colour > 0 ? gain : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        histL[(size_t) hpos] = ch[0][i]; histR[(size_t) hpos] = ch[1][i];
        const int r1 = (hpos - kTapDelay + len) % len, r2 = (hpos - t2 + len) % len;
        const float al = histL[(size_t) r1], ar = histR[(size_t) r1], el = histL[(size_t) r2], er = histR[(size_t) r2];
        hpos = (hpos + 1) % len;
        outL[i] = g * (kTap1 * (al + ar) + kTap2 * (el - kTap2Cross * er));
        outR[i] = g * (kTap1 * (ar - al) + kTap2 * (kTap2Cross * el + er));
    }
}

//==============================================================================
namespace
{
// each voice's delay d(t) = centre + amp cos (2 pi f t + phase), t from the plug-in's start, as tracked on the reference
// (Stereo's both voices and Wide's right one to 0.1 .. 0.3 ms; the others, with larger swings, only roughly: their
// rates set to the preset's LFO rate / 2), and its gain (the measured wet level on white noise)
struct Voice { double centre, amp, f, phase, gain; };
struct DoublerColour { Voice v[2]; bool shelves; };
constexpr DoublerColour doublerColours[4] = {
    { { { 10.065, 2.616, 0.1998, 3.202, 0.0 }, { 16.717, 1.742, 0.3, 3.216, 0.0 } }, true },
    { { { 10.065, 2.616, 0.1998, 3.202, 0.2655 }, { 16.717, 1.742, 0.3, 3.216, 0.2650 } }, true },   // Stereo ("Tight")
    { { { 6.00, 2.17, 0.2, 2.34, 0.2651 }, { 35.26, 4.26, 0.3035, 6.13, 0.2660 } }, true },          // Wide ("Medium")
    { { { 8.35, 3.07, 0.15, 5.17, 0.2668 }, { 6.33, 3.01, 0.15, 1.33, 0.2890 } }, false } };         // Spreader ("Wide")
}

void Doubler::prepare (double sampleRate)
{
    fs = sampleRate;
    lineL.assign ((size_t) (fs * 0.1) + 8, 0.0f); lineR.assign (lineL.size(), 0.0f);
    pos = 0; t = 0.0; running = 1; shelvesFor = -1;
    for (auto& b : shelves) b.reset();
}

void Doubler::process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain)
{
    if (colour > 0) running = colour;
    const auto& dc = doublerColours[running];
    if (shelvesFor != running)
    {
        shelvesFor = running;
        double c[5];
        rbj (eq::lowShelf, 400.0, 0.7071, -8.6, fs, c); shelves[0].set (c);
        rbj (eq::highShelf, 4000.0, 0.7071, 4.5, fs, c); shelves[1].set (c);
    }
    const int len = (int) lineL.size();
    const double g = colour > 0 ? gain : 0.0;
    auto read = [len] (const std::vector<float>& b, int p, double d)       // a 4-point Hermite read d samples back
    {
        const int i = (int) d; const double fr = d - i;
        auto at = [&] (int k) { return (double) b[(size_t) ((p - k + 2 * len) % len)]; };
        const double y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * fr + c2) * fr + c1) * fr + y1;
    };
    for (int i = 0; i < n; ++i)
    {
        lineL[(size_t) pos] = inL[i]; lineR[(size_t) pos] = inR[i];
        const double ts = t / fs;
        double v[2];
        for (int k = 0; k < 2; ++k)
        {
            const double dms = dc.v[k].centre + dc.v[k].amp * std::cos (2.0 * juce::MathConstants<double>::pi * dc.v[k].f * ts + dc.v[k].phase);
            v[k] = dc.v[k].gain * read (k == 0 ? lineL : lineR, pos, juce::jlimit (1.0, (double) len - 4.0, dms * fs / 1000.0));
            if (dc.shelves) { v[k] = shelves[0].run (v[k], k); v[k] = shelves[1].run (v[k], k); }
        }
        pos = (pos + 1) % len;
        t += 1.0;
        outL[i] = (float) (g * v[0]); outR[i] = (float) (g * v[1]);
    }
}

//==============================================================================
void Chain::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    const int k = fs > 144000.0 ? 4 : fs > 72000.0 ? 2 : 1;
    latency = 193 * k - (k - 1);
    compressor.prepare (fs);
    const int inputDelay = latency - compressor.lookAhead();         // (the look-ahead is the rest of the latency)
    lineL.assign ((size_t) inputDelay + 1, 0.0f); lineR.assign ((size_t) inputDelay + 1, 0.0f);
    pos = 0;
    // the fixed filter at the nearest fitted rate
    const fixed::Rate* best = &fixed::rates[0];
    for (const auto& r : fixed::rates) if (std::abs (std::log (r.fs / fs)) < std::abs (std::log (best->fs / fs))) best = &r;
    for (int i = 0; i < 4; ++i)
    {
        fixedFilter[(size_t) i].set (best->sos[i]); fixedFilter[(size_t) i].reset();
        sendFilter[(size_t) i].set (best->sos[i]); sendFilter[(size_t) i].reset();
    }
    bass.prepare (fs, true); treble.prepare (fs, false);
    reverb.prepare (fs, maxBlock);
    delays.prepare (fs, maxBlock);
    doubler.prepare (fs);
    for (auto* bf : { &tap, &send, &wet, &wet2, &wet3 }) bf->setSize (2, jmax (1, maxBlock));
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
    if (tap.getNumSamples() < n) for (auto* bf : { &tap, &send, &wet, &wet2, &wet3 }) bf->setSize (2, n, false, false, true);
    std::normal_distribution<double> gauss (0.0, 1.0);
    const int len = (int) lineL.size();
    // the input, Sensitivity, Balance, the Noise, the input delay, the clear chain's shaper, Bass, Treble (into tap)
    for (int i = 0; i < n; ++i)
    {
        double xl = l[i] * gIn, xr = r[i] * gIn;
        level = (0.5 * (xl * xl + xr * xr)) + levelCoef * (level - 0.5 * (xl * xl + xr * xr));
        xl *= balL; xr *= balR;
        if (noiseOn) { xl += noiseGain * gauss (noise); xr += noiseGain * gauss (noise); }
        lineL[(size_t) pos] = (float) xl; lineR[(size_t) pos] = (float) xr;
        pos = (pos + 1) % len;
        xl = lineL[(size_t) pos]; xr = lineR[(size_t) pos];
        // the clear chain's own shaper (measured: x - 0.00145 x^2 + 0.00146 x^3, before the fixed filter)
        xl = xl - 0.00144547 * xl * xl + 0.00145842 * xl * xl * xl;
        xr = xr - 0.00144547 * xr * xr + 0.00145842 * xr * xr * xr;
        bass.run (xl, xr);
        treble.run (xl, xr);
        tap.setSample (0, i, (float) xl); tap.setSample (1, i, (float) xr);
    }
    // Compress (its look-ahead completes the latency); the de-esser still to come
    compressor.process (tap.getWritePointer (0), tap.getWritePointer (1), n, p.colour[2], p.fader[2]);
    for (int i = 0; i < n; ++i)
    {
        double xl = tap.getSample (0, i), xr = tap.getSample (1, i);
        for (auto& f : fixedFilter) { xl = f.run (xl, 0); xr = f.run (xr, 1); }
        l[i] = (float) xl; r[i] = (float) xr;
    }
    // the wet stages on the tap: the reverb (1.2 dB per fader step from its measured response at 0)
    reverb.process (tap.getReadPointer (0), tap.getReadPointer (1), wet.getWritePointer (0), wet.getWritePointer (1), n,
                    p.colour[3], (float) std::pow (10.0, 1.2 * p.fader[3] / 20.0));
    for (int i = 0; i < n; ++i)                                       // the send for the delays and the doubler
    {
        double sl = tap.getSample (0, i), sr = tap.getSample (1, i);
        for (auto& f : sendFilter) { sl = f.run (sl, 0); sr = f.run (sr, 1); }
        send.setSample (0, i, (float) sl); send.setSample (1, i, (float) sr);
    }
    delays.process (send.getReadPointer (0), send.getReadPointer (1), wet2.getWritePointer (0), wet2.getWritePointer (1), n,
                    p.colour[4], (float) std::pow (10.0, 1.2 * p.fader[4] / 20.0), p.bpm);
    doubler.process (send.getReadPointer (0), send.getReadPointer (1), wet3.getWritePointer (0), wet3.getWritePointer (1), n,
                     p.colour[5], (float) std::pow (10.0, 1.2 * p.fader[5] / 20.0));
    for (int i = 0; i < n; ++i)
    {
        l[i] = (float) ((l[i] + wet.getSample (0, i) + wet2.getSample (0, i) + wet3.getSample (0, i)) * gOut);
        r[i] = (float) ((r[i] + wet.getSample (1, i) + wet2.getSample (1, i) + wet3.getSample (1, i)) * gOut);
    }
}
}
