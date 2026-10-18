// Unit tests for WVox: the DSP (WVoxDsp.h) and the plug-in processor. Run: WVoxTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WVoxDsp.h"
#include "WVoxProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

static const double kPi = 3.14159265358979323846;

static std::vector<float> tone (double fs, double f, double secs, double db)
{
    std::vector<float> x ((size_t) (fs * secs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (std::pow (10.0, db / 20.0) * std::sin (2.0 * kPi * f * (double) i / fs));
    return x;
}

// renders x through the DSP, latency compensated (the output lines up with the input)
static std::vector<float> run (const std::vector<float>& x, double fs, wvox::Processor::Params p, int block = 512)
{
    wvox::Processor proc;
    proc.prepare (fs, 1);
    proc.setParams (p);
    const int lat = proc.getLatency();
    std::vector<float> in (x); in.resize (x.size() + (size_t) lat, 0.0f);
    std::vector<float> out (in.size());
    for (size_t s = 0; s < in.size(); s += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - s);
        const float* ip[1] = { in.data() + s }; float* op[1] = { out.data() + s };
        proc.process (ip, op, n);
    }
    return { out.begin() + lat, out.end() };
}

static double rmsDb (const std::vector<float>& x, size_t from)
{
    double s = 0; for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / (double) (x.size() - from) + 1.0e-30);
}

static double steadyGainDb (double comp, double level, double gate = -80.0, double gain = 0.0)
{
    const double fs = 48000.0;
    auto x = tone (fs, 1000.0, 1.6, level);
    wvox::Processor::Params p; p.compDb = (float) comp; p.gateDb = (float) gate; p.gainDb = (float) gain;
    auto y = run (x, fs, p);
    return rmsDb (y, (size_t) (fs * 1.2)) - rmsDb (x, (size_t) (fs * 1.2));
}

static void testLatency()
{
    CHECK (wvox::latencyFor (44100.0) == 64);  CHECK (wvox::latencyFor (48000.0) == 64);
    CHECK (wvox::latencyFor (88200.0) == 128); CHECK (wvox::latencyFor (96000.0) == 128);
    CHECK (wvox::latencyFor (176400.0) == 256); CHECK (wvox::latencyFor (192000.0) == 256);
    // an impulse comes out exactly `latency` samples later
    wvox::Processor proc; proc.prepare (48000.0, 1);
    std::vector<float> in (1000, 0.0f), out (1000);
    in[100] = 0.05f;
    const float* ip[1] = { in.data() }; float* op[1] = { out.data() };
    proc.process (ip, op, 1000);
    size_t pk = 0; for (size_t i = 0; i < out.size(); ++i) if (std::fabs (out[i]) > std::fabs (out[pk])) pk = i;
    CHECK ((int) pk == 100 + proc.getLatency());
}

static void testStaticLaw()
{
    // measured law: gain = 0.99 M / (1 + (M - 1) x), M = 10^(-comp/20), x = peak amplitude of a steady sine
    for (double comp : { 0.0, -6.0, -12.0, -24.0, -36.0 })
        for (double level : { -60.0, -30.0, -20.0, -10.0, -3.0 })
        {
            const double M = std::pow (10.0, -comp / 20.0), x = std::pow (10.0, level / 20.0);
            const double expect = 20.0 * std::log10 (0.99 * M / (1.0 + (M - 1.0) * x));
            CHECK_NEAR (steadyGainDb (comp, level), expect, 0.05);
        }
    CHECK_NEAR (steadyGainDb (0.0, -20.0), -0.0873, 0.01);                                              // the fixed x0.99
    CHECK_NEAR (steadyGainDb (-24.0, -20.0, -80.0, -6.0) - steadyGainDb (-24.0, -20.0), -6.0, 0.01);    // Gain is a trim
}

static void testGate()
{
    // hard-knee 2:1 expander: gain = level - threshold below the threshold, 0 above (comp 0 adds the fixed -0.0873 dB)
    const double fixedDb = -0.0873;
    CHECK_NEAR (steadyGainDb (0.0, -50.0, -40.0), -10.0 + fixedDb, 0.3);
    CHECK_NEAR (steadyGainDb (0.0, -70.0, -40.0), -30.0 + fixedDb, 0.3);
    CHECK_NEAR (steadyGainDb (0.0, -30.0, -40.0), fixedDb, 0.05);
    CHECK_NEAR (steadyGainDb (0.0, -20.0, -80.0), fixedDb, 0.05);               // -80 = gate off
}

static void testBehaviour()
{
    const double fs = 48000.0;
    wvox::Processor::Params p; p.compDb = -24.0f;
    // attack is immediate: a short loud burst after silence gets its full reduction at once
    std::vector<float> x (24000, 0.0f);
    auto b = tone (fs, 3000.0, 0.01, -10.0);
    for (size_t i = 0; i < b.size(); ++i) x[4800 + i] = b[i];
    auto y = run (x, fs, p);
    float pkIn = 0, pkOut = 0;
    for (size_t i = 4800; i < 4800 + b.size(); ++i) { pkIn = std::max (pkIn, std::fabs (x[i])); pkOut = std::max (pkOut, std::fabs (y[i])); }
    CHECK_NEAR (20.0 * std::log10 (pkOut / pkIn), 8.8, 0.5);

    // release is slow: 100 ms after a long loud tone ends the gain is still far from its quiet value
    auto loud = tone (fs, 3000.0, 1.0, -10.0), quiet = tone (fs, 3000.0, 1.0, -50.0);
    loud.insert (loud.end(), quiet.begin(), quiet.end());
    auto z = run (loud, fs, p);
    auto g = [&] (size_t at) { double a = 0, c = 0; for (size_t i = at; i < at + 480; ++i) { a += (double) z[i] * z[i]; c += (double) loud[i] * loud[i]; } return 10.0 * std::log10 (a / c); };
    CHECK (g ((size_t) (fs * 1.1)) < g ((size_t) (fs * 1.9)) - 3.0);

    // output stays finite on silence, denormals, huge and odd input
    std::vector<float> weird (4096, 0.0f);
    for (size_t i = 0; i < weird.size(); ++i) weird[i] = (i % 7 == 0) ? 1.0e30f : (i % 5 == 0 ? 1.0e-40f : (i % 3 == 0 ? -3.0f : 0.0f));
    auto w = run (weird, fs, p);
    bool finite = true; for (float v : w) finite = finite && std::isfinite (v);
    CHECK (finite);
}

static void testBlockSizeInvariance()
{
    const double fs = 48000.0;
    auto x = tone (fs, 440.0, 1.0, -20.0);
    for (size_t i = 0; i < x.size(); ++i) x[i] += 0.05f * std::sin (2.0f * 3.14159f * 61.0f * (float) i / 48000.0f) * ((i / 9000) % 2 ? 1.0f : 0.1f);
    wvox::Processor::Params p; p.compDb = -18.0f; p.gateDb = -50.0f;
    auto a = run (x, fs, p, 512), b = run (x, fs, p, 64), c = run (x, fs, p, 37);
    double d1 = 0, d2 = 0;
    for (size_t i = 0; i < a.size(); ++i) { d1 = std::max (d1, (double) std::fabs (a[i] - b[i])); d2 = std::max (d2, (double) std::fabs (a[i] - c[i])); }
    CHECK (d1 == 0.0);
    CHECK (d2 == 0.0);
}

static void testStereoLink()
{
    wvox::Processor proc; proc.prepare (48000.0, 2);
    wvox::Processor::Params p; p.compDb = -24.0f; proc.setParams (p);
    auto l = tone (48000.0, 1000.0, 1.0, -10.0), r = tone (48000.0, 1300.0, 1.0, -40.0);
    std::vector<float> ol (l.size()), orr (l.size());
    const float* ip[2] = { l.data(), r.data() }; float* op[2] = { ol.data(), orr.data() };
    proc.process (ip, op, (int) l.size());
    // the quiet right channel gets the gain of the loud left one (+8.8 dB), not its own (+22 dB)
    double a = 0, c = 0; for (size_t i = 30000; i < 48000; ++i) { a += (double) orr[i] * orr[i]; c += (double) r[i - 64] * r[i - 64]; }
    CHECK_NEAR (10.0 * std::log10 (a / c), 8.8, 0.3);
}

static void testProcessor()
{
    WVoxProcessor proc;
    auto* comp = proc.apvts.getParameter (wvox::comp);
    auto* gate = proc.apvts.getParameter (wvox::gate);
    auto* gain = proc.apvts.getParameter (wvox::gain);
    CHECK (comp != nullptr && gate != nullptr && gain != nullptr);
    CHECK_NEAR (comp->convertFrom0to1 (comp->getDefaultValue()), 0.0, 1e-4);
    CHECK_NEAR (gate->convertFrom0to1 (gate->getDefaultValue()), -80.0, 1e-4);
    CHECK_NEAR (gain->convertFrom0to1 (gain->getDefaultValue()), 0.0, 1e-4);
    CHECK_NEAR (comp->convertFrom0to1 (0.0f), -36.0, 1e-4);
    CHECK (comp->getText (comp->convertTo0to1 (-12.0f), 10).contains ("-12.0"));

    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    CHECK (proc.getLatencySamples() == 64);
    proc.prepareToPlay (96000.0, 512);
    CHECK (proc.getLatencySamples() == 128);
    proc.prepareToPlay (48000.0, 512);

    // processBlock gives the same audio as the bare DSP
    comp->setValueNotifyingHost (comp->convertTo0to1 (-24.0f));
    auto x = tone (48000.0, 1000.0, 0.5, -20.0);
    juce::AudioBuffer<float> buf (2, (int) x.size());
    for (int c = 0; c < 2; ++c) std::copy (x.begin(), x.end(), buf.getWritePointer (c));
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);
    wvox::Processor::Params p; p.compDb = -24.0f;
    wvox::Processor ref; ref.prepare (48000.0, 2); ref.setParams (p);
    std::vector<float> o0 (x.size()), o1 (x.size());
    const float* ip[2] = { x.data(), x.data() }; float* op[2] = { o0.data(), o1.data() };
    ref.process (ip, op, (int) x.size());
    double d = 0; for (size_t i = 0; i < x.size(); ++i) d = std::max (d, (double) std::fabs (buf.getSample (0, (int) i) - o0[i]));
    CHECK (d < 1.0e-6);

    // state round trip
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    WVoxProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (other.apvts.getRawParameterValue (wvox::comp)->load(), -24.0, 1e-3);

    // Mono mode on a stereo bus: independent channels
    auto* mode = proc.apvts.getParameter (wvox::mode);
    mode->setValueNotifyingHost (1.0f);
    juce::AudioBuffer<float> b2 (2, 24000);
    auto loud = tone (48000.0, 1000.0, 0.5, -10.0), quiet = tone (48000.0, 1300.0, 0.5, -40.0);
    std::copy (loud.begin(), loud.end(), b2.getWritePointer (0)); std::copy (quiet.begin(), quiet.end(), b2.getWritePointer (1));
    proc.prepareToPlay (48000.0, 512);
    proc.processBlock (b2, midi);
    double a = 0, c = 0; for (int i = 15000; i < 24000; ++i) { a += (double) b2.getSample (1, i) * b2.getSample (1, i); c += (double) quiet[(size_t) i - 64] * quiet[(size_t) i - 64]; }
    CHECK (10.0 * std::log10 (a / c) > 15.0);          // the quiet channel keeps its own, larger, gain
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testLatency();
    testStaticLaw();
    testGate();
    testBehaviour();
    testBlockSizeInvariance();
    testStereoLink();
    testProcessor();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
