// Unit tests for WBass: the DSP (WBassDsp.h) and the plug-in processor. Run: WBassTests (exit code 0 = all passed).
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WBassDsp.h"
#include "WBassProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

static const double kPi = 3.14159265358979323846;
static const double fs = 48000.0;

static std::vector<float> tone (double f, double secs, double db)
{
    std::vector<float> x ((size_t) (fs * secs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (std::pow (10.0, db / 20.0) * std::sin (2.0 * kPi * f * (double) i / fs));
    return x;
}

struct Settings { double freq = 80, intensity = 0, gain = 0, decay = 1; bool origIn = true; };

static std::vector<float> run (const std::vector<float>& x, Settings s, int block = 512)
{
    wbass::Processor p;
    p.setFreq (s.freq); p.setIntensityDb (s.intensity); p.setGainDb (s.gain); p.setDecay (s.decay); p.setOriginalBass (s.origIn);
    p.prepare (fs, 1);
    std::vector<float> y (x);
    for (size_t i = 0; i < y.size(); i += (size_t) block)
    {
        float* ch[1] = { y.data() + i };
        p.process (ch, 1, (int) std::min ((size_t) block, y.size() - i));
    }
    return y;
}

// complex amplitude of frequency f over the last `secs` (whole periods)
static std::complex<double> component (const std::vector<float>& y, double f, double secs = 0.5)
{
    const size_t n = (size_t) (std::round (std::floor (secs * f) * fs / f));
    std::complex<double> acc;
    for (size_t i = y.size() - n; i < y.size(); ++i) acc += (double) y[i] * std::polar (1.0, -2.0 * kPi * f * (double) i / fs);
    return 2.0 * acc / (double) n;
}

static double db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }

static void testOriginalPath()
{
    // Intensity -inf: only the original path. In = all-pass (unity), Out = HP3(Freq)
    Settings s; s.intensity = -400;
    for (double f : { 20.0, 80.0, 300.0, 1000.0 })
    {
        auto x = tone (f, 1.0, -20);
        s.origIn = true;  CHECK_NEAR (db (std::abs (component (run (x, s), f))), -20.0, 0.01);
        s.origIn = false;
        const double expect = -20.0 - 10.0 * std::log10 (1.0 + std::pow (80.0 / f, 6.0));
        CHECK_NEAR (db (std::abs (component (run (x, s), f))), expect, 0.05);
    }
    // latency 0: an impulse comes out at once
    std::vector<float> imp (4800, 0.0f); imp[0] = 0.5f;
    s.origIn = true;
    auto y = run (imp, s);
    CHECK (std::fabs (y[0]) > 0.1f);
}

static void testGeneratedPath()
{
    // Intensity is an exact gain on the generated part (below the clip)
    auto x = tone (50.0, 1.5, -30);
    Settings a; a.origIn = false; a.intensity = -400;
    Settings b = a; b.intensity = 0;
    Settings c = a; c.intensity = 6;
    auto y0 = run (x, a), y1 = run (x, b), y2 = run (x, c);
    double worst = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double g1 = (double) y1[i] - y0[i], g2 = (double) y2[i] - y0[i];
        worst = std::max (worst, std::fabs (g2 - g1 * std::pow (10.0, 6.0 / 20.0)));
    }
    CHECK (worst < 1.0e-5);

    // low-level gain of the generated fundamental at Freq: C0 |LP3(1/3) HP3(1)^2| (about +14.9 dB at Freq)
    {
        wbass::Tuning t;
        auto lo = tone (80.0, 1.5, -70);
        Settings s; s.origIn = false; s.intensity = 0;
        Settings s0 = s; s0.intensity = -400;
        auto g = run (lo, s), g0 = run (lo, s0);
        std::vector<float> gen (g.size()); for (size_t i = 0; i < g.size(); ++i) gen[i] = g[i] - g0[i];
        const double expect = db (t.C0) - 10.0 * std::log10 (1.0 + std::pow (3.0, 6.0)) - 2.0 * 10.0 * std::log10 (2.0);
        CHECK_NEAR (db (std::abs (component (gen, 80.0))) + 70.0, expect, 0.5);
    }

    // Decay 0 removes the even harmonics (the harmonic product), Decay 1 makes them
    auto t = tone (40.0, 1.5, -20);
    Settings d1; d1.origIn = false; Settings d0 = d1; d0.decay = 0;
    const double h2on = std::abs (component (run (t, d1), 80.0)), h2off = std::abs (component (run (t, d0), 80.0));
    CHECK (db (h2on) > -60.0);
    CHECK (db (h2off) < -120.0);
}

static void testOutput()
{
    // hard clip to the 16-bit range
    auto x = tone (1000.0, 0.3, 12);
    Settings s; s.intensity = -400;
    auto y = run (x, s);
    float mx = -2, mn = 2; for (float v : y) { mx = std::max (mx, v); mn = std::min (mn, v); }
    CHECK_NEAR (mx, 32767.0 / 32768.0, 1.0e-7);
    CHECK_NEAR (mn, -1.0, 1.0e-7);

    // Gain trim (after the glide)
    auto q = tone (1000.0, 1.0, -20);
    Settings g; g.intensity = -400; g.gain = -12;
    CHECK_NEAR (db (std::abs (component (run (q, g), 1000.0))), -32.0, 0.01);

    // block size does not change anything
    auto r = tone (55.0, 1.0, -12);
    Settings b; b.origIn = false;
    auto y1 = run (r, b, 512), y2 = run (r, b, 37), y3 = run (r, b, 1);
    double d2 = 0, d3 = 0;
    for (size_t i = 0; i < r.size(); ++i) { d2 = std::max (d2, (double) std::fabs (y1[i] - y2[i])); d3 = std::max (d3, (double) std::fabs (y1[i] - y3[i])); }
    CHECK (d2 == 0.0);
    CHECK (d3 == 0.0);

    // nothing in, nothing out; everything finite under a loud low burst
    std::vector<float> z (48000, 0.0f);
    auto yz = run (z, b);
    double m = 0; for (float v : yz) m = std::max (m, (double) std::fabs (v));
    CHECK (m == 0.0);
    auto loud = tone (25.0, 1.0, 0);
    bool finite = true; for (float v : run (loud, b)) finite = finite && std::isfinite (v);
    CHECK (finite);
}

static void testStereo()
{
    // identical channels give identical outputs; the generated bass comes from the channel average
    auto x = tone (45.0, 1.0, -20);
    wbass::Processor p; p.setOriginalBass (false); p.prepare (fs, 2);
    std::vector<float> l (x), r (x);
    float* ch[2] = { l.data(), r.data() };
    p.process (ch, 2, (int) x.size());
    double d = 0; for (size_t i = 0; i < x.size(); ++i) d = std::max (d, (double) std::fabs (l[i] - r[i]));
    CHECK (d == 0.0);
    auto mono = run (x, Settings { 80, 0, 0, 1, false });
    double e = 0; for (size_t i = 0; i < x.size(); ++i) e = std::max (e, (double) std::fabs (l[i] - mono[i]));
    CHECK (e < 1.0e-6);
}

static void testProcessor()
{
    WBassProcessor proc;
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbass::freq)->load(), 80.0, 1e-4);
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbass::ratio)->load(), 0.63, 1e-4);
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbass::decay)->load(), 1.0, 1e-4);
    CHECK (proc.apvts.getRawParameterValue (wbass::origBass)->load() > 0.5f);
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 48000);
    auto x = tone (50.0, 1.0, -20);
    for (int c = 0; c < 2; ++c) std::copy (x.begin(), x.end(), buf.getWritePointer (c));
    juce::MidiBuffer midi;
    for (int s = 0; s < 48000; s += 512)
    {
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 2, s, std::min (512, 48000 - s));
        proc.processBlock (blk, midi);
    }
    auto ref = run (x, Settings{});
    double d = 0; for (int i = 0; i < 48000; ++i) d = std::max (d, (double) std::fabs (buf.getSample (0, i) - ref[(size_t) i]));
    CHECK (d < 1.0e-6);

    // hidden parameters survive the state
    proc.apvts.getParameter (wbass::decay)->setValueNotifyingHost (0.0f);
    proc.apvts.getParameter (wbass::origBass)->setValueNotifyingHost (0.0f);
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    WBassProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (other.apvts.getRawParameterValue (wbass::decay)->load(), 0.0, 1e-4);
    CHECK (other.apvts.getRawParameterValue (wbass::origBass)->load() < 0.5f);
}

static void testFactoryPresets()
{
    WBassProcessor proc;
    const auto& list = wbass::factoryPresets();
    CHECK (! list.empty());
    CHECK (proc.getNumPrograms() == 1 + (int) list.size());
    for (size_t i = 0; i < list.size(); ++i)
    {
        for (const auto& v : list[i].values)
        {
            auto* p = proc.apvts.getParameter (v.id);
            CHECK (p != nullptr);                                        // every id exists
            if (p == nullptr) continue;
            const auto& r = p->getNormalisableRange();
            CHECK (v.value >= r.start - 1.0e-4f && v.value <= r.end + 1.0e-4f);
        }
        proc.setCurrentProgram ((int) i + 1);
        for (const auto& v : list[i].values)
            if (auto* p = proc.apvts.getParameter (v.id))
                CHECK (std::abs (p->convertFrom0to1 (p->getValue()) - v.value) <= 0.051f);   // applied (1 Hz / 0.1 dB steps)
    }
    proc.setCurrentProgram (0);
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            CHECK (std::abs (rp->getValue() - rp->getDefaultValue()) < 1.0e-6f);   // program 0 = defaults
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testOriginalPath();
    testGeneratedPath();
    testOutput();
    testStereo();
    testProcessor();
    testFactoryPresets();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
