// Unit tests for WComp: the DSP (WCompDsp.h, WCompAnalog.h) and the plug-in processor. Run: WCompTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WCompDsp.h"
#include "WCompAnalog.h"
#include "WCompProcessor.h"
#include "WCompNeedle.h"
#include "WavyFlatUI.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

static const double kPi = 3.14159265358979323846;
using Settings = wcomp::Compressor::Settings;

static std::vector<float> tone (double f, double secs, double db, double fs = 48000.0)
{
    std::vector<float> x ((size_t) (fs * secs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (std::pow (10.0, db / 20.0) * std::sin (2.0 * kPi * f * (double) i / fs));
    return x;
}

// stereo in place; returns the output aligned with the input (latency removed)
static std::vector<std::vector<float>> run (std::vector<std::vector<float>> x, const Settings& s, int block = 512, double fs = 48000.0)
{
    wcomp::Compressor c;
    c.prepare (fs, (int) x.size());
    c.setSettings (s);
    const int lat = c.latency();
    for (auto& v : x) v.resize (v.size() + (size_t) lat, 0.0f);
    const size_t n = x[0].size();
    for (size_t i = 0; i < n; i += (size_t) block)
    {
        float* ch[2] = { x[0].data() + i, x.size() > 1 ? x[1].data() + i : nullptr };
        c.process (ch, (int) std::min ((size_t) block, n - i));
    }
    for (auto& v : x) v.erase (v.begin(), v.begin() + lat);
    return x;
}

static double rms (const std::vector<float>& v, size_t from = 0)
{
    double a = 0.0;
    for (size_t i = from; i < v.size(); ++i) a += (double) v[i] * v[i];
    return std::sqrt (a / (double) std::max<size_t> (1, v.size() - from));
}
static double db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }
static double maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i) m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

static void testLatency()
{
    wcomp::Compressor c;
    c.prepare (44100.0, 2); CHECK (c.latency() == 64);
    c.prepare (48000.0, 2); CHECK (c.latency() == 64);
    c.prepare (96000.0, 2); CHECK (c.latency() == 128);
    c.prepare (192000.0, 2); CHECK (c.latency() == 256);
}

static void testIdentity()
{
    // threshold 0 dB, signal below it: no reduction and no makeup (makeup starts below -9 dB), so the output is the input
    Settings s; s.thresholdDb = 0.0f;
    auto x = tone (1000.0, 0.5, -6.0);
    auto y = run ({ x, x }, s);
    CHECK (maxDiff (x, y[0]) < 1.0e-6);
    // ratio 1: the same
    Settings r; r.ratio = 1.0f; r.thresholdDb = -40.0f;
    y = run ({ x, x }, r);
    CHECK_NEAR (db (rms (y[0], 4800)) - db (rms (x, 4800)), wcomp::Compressor::makeupDb (-40.0, 1.0) * wcomp::Compressor::makeupScale (100.0), 0.01);
}

static void testStatic()
{
    // a steady tone: the reduction is h(x) of the static curve and the makeup follows the tables
    Settings s; s.thresholdDb = -30.0f; s.ratio = 4.0f;
    auto x = tone (1000.0, 2.0, -10.0);
    auto y = run ({ x, x }, s);
    const double gain = db (rms (y[0], 72000)) - db (rms (x, 72000));
    const double makeup = wcomp::Compressor::makeupDb (-30.0, 4.0) * wcomp::Compressor::makeupScale (100.0);
    const double xx = 0.75 * (20.0 * std::log10 (std::pow (10.0, -0.5) + 0.001) + 30.0);
    // h is within 0.6 dB of x (the knee); the release ripple adds a little
    CHECK (gain - makeup < -xx + 0.8 && gain - makeup > -xx - 0.3);
}

static void testBlockIndependence()
{
    Settings s; s.thresholdDb = -30.0f; s.ratio = 10.0f; s.attackMs = 0.5f; s.releaseMs = 30.0f; s.punch = 5.0f; s.outputDb = 12.0f;
    std::vector<float> x ((size_t) 48000);
    uint32_t r = 1;
    for (size_t i = 0; i < x.size(); ++i) { r = r * 1664525u + 1013904223u; x[i] = (float) (((double) (r >> 8) / 16777216.0 - 0.5) * (i % 9000 < 3000 ? 1.2 : 0.05)); }
    auto a = run ({ x, x }, s, 512), b = run ({ x, x }, s, 1), c = run ({ x, x }, s, 333);
    CHECK (maxDiff (a[0], b[0]) < 1.0e-6);
    CHECK (maxDiff (a[0], c[0]) < 1.0e-6);
}

static void testLinkAndLimiter()
{
    // linked: a loud left channel reduces the quiet right one as much
    Settings s; s.thresholdDb = -30.0f; s.ratio = 10.0f;
    auto loud = tone (100.0, 1.0, -6.0), quiet = tone (1000.0, 1.0, -40.0);
    auto y = run ({ loud, quiet }, s);
    const double gl = db (rms (y[0], 24000)) - db (rms (loud, 24000)), gr = db (rms (y[1], 24000)) - db (rms (quiet, 24000));
    CHECK_NEAR (gl, gr, 0.5);
    CHECK (gl < -5.0);
    // the output limiter (Off and On) keeps the peaks at full scale
    for (bool on : { false, true })
    {
        Settings t; t.outputDb = 18.0f; t.limiter = on;
        auto z = run ({ loud, loud }, t);
        float pk = 0.0f;
        for (size_t i = 4800; i < z[0].size(); ++i) pk = std::max (pk, std::abs (z[0][i]));
        CHECK (pk < 1.01f);                               // the reference keeps 0.999 here; the estimate overshoots a little
        CHECK (pk > 0.98f);
    }
}

static void testMix()
{
    // Mix 0: the dry signal (delayed), times the output gain
    Settings s; s.thresholdDb = -40.0f; s.ratio = 20.0f; s.mix = 0.0f; s.outputDb = -6.0f;
    auto x = tone (440.0, 0.5, -12.0);
    auto y = run ({ x, x }, s);
    std::vector<float> e (x);
    for (auto& v : e) v *= (float) std::pow (10.0, -6.0 / 20.0);
    CHECK (maxDiff (e, y[0]) < 1.0e-6);
}

static void testSync()
{
    CHECK_NEAR (wcomp::syncedReleaseMs (5, 120.0), 93.75, 1e-3);          // 1/32D at 120
    CHECK_NEAR (wcomp::syncedReleaseMs (13, 0.0), 500.0, 1e-3);           // 1/4, no tempo: 120
    CHECK_NEAR (wcomp::syncedReleaseMs (19, 122.0), 8.0 * 60000.0 / 122.0, 1e-2);   // 2 bars allowed at 122
    CHECK_NEAR (wcomp::syncedReleaseMs (19, 120.0), 2000.0, 1e-3);        // ... not at 120: 1 bar
    CHECK_NEAR (wcomp::syncedReleaseMs (18, 72.0), 4.0 * 60000.0 / 72.0, 1e-2);     // 1 bar at 72
    CHECK_NEAR (wcomp::syncedReleaseMs (18, 70.0), 3.0 * 60000.0 / 70.0, 1e-2);     // ... at 70: 1/2D
    CHECK_NEAR (wcomp::syncedReleaseMs (17, 50.0), 2400.0, 1e-2);         // 1/2D at 50: 1/2
    CHECK_NEAR (wcomp::syncedReleaseMs (16, 35.0), 1.5 * 60000.0 / 35.0, 1e-2);     // 1/2 at 35: 1/4D
    CHECK_NEAR (wcomp::syncedReleaseMs (15, 30.0), 4.0 / 3.0 * 2000.0, 1e-2);       // triplets are not too long
}

static void testAnalog()
{
    // Off: an identity
    {
        wcomp::Analog a; a.prepare (48000.0, 2); a.setMode (0);
        auto x = tone (1000.0, 0.2, -20.0), y = x;
        float* ch[1] = { y.data() };
        a.process (ch, (int) y.size());
        CHECK (maxDiff (x, y) == 0.0);
    }
    // the noise with silence in (measured: -101.7, -82.7, -101.2, -108.8 dBFS)
    const double expected[] = { -101.7, -82.7, -101.2, -108.8 };
    for (int m = 1; m <= 4; ++m)
    {
        wcomp::Analog a; a.prepare (48000.0, 2); a.setMode (m);
        std::vector<float> l ((size_t) 96000, 0.0f), r (l);
        float* ch[2] = { l.data(), r.data() };
        a.process (ch, (int) l.size());
        CHECK_NEAR (db (rms (l, 4800)), expected[m - 1], 1.5);
        double c = 0.0;
        for (size_t i = 4800; i < l.size(); ++i) c += (double) l[i] * r[i];
        CHECK (std::abs (c / (rms (l, 4800) * rms (r, 4800) * (double) (l.size() - 4800))) < 0.05);   // independent channels
    }
    // mode 4: the crossover offset (about 0.001) on a quiet tone, little change on a loud one
    {
        wcomp::Analog a; a.prepare (48000.0, 1); a.setMode (4);
        auto x = tone (100.0, 0.5, -20.0), y = x;
        float* ch[1] = { y.data() };
        a.process (ch, (int) y.size());
        double off = 0.0; int n = 0;
        for (size_t i = 4800; i < x.size(); ++i) if (std::abs (x[i]) > 0.08f) { off += (y[i] - x[i]) * (x[i] > 0 ? 1.0 : -1.0); ++n; }
        CHECK_NEAR (off / n, -0.001, 0.0002);
    }
}

static void testProcessor()
{
    WCompProcessor proc;
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::stereo());
    l.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK (proc.setBusesLayout (l));
    proc.prepareToPlay (48000.0, 512);
    CHECK (proc.getLatencySamples() == 64);
    auto set = [&proc] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (wcomp::analog, 0.0f); set (wcomp::threshold, -30.0f); set (wcomp::ratio, 4.0f);

    // the side-chain bus is ignored: the same output with or without it fed
    auto x = tone (300.0, 0.5, -10.0);
    auto render = [&] (bool feedSc)
    {
        proc.prepareToPlay (48000.0, 512);
        std::vector<float> out;
        juce::AudioBuffer<float> b (4, 512);
        juce::MidiBuffer midi;
        for (size_t p = 0; p + 512 <= x.size(); p += 512)
        {
            for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, x.data() + p, 512);
            for (int c = 2; c < 4; ++c) if (feedSc) b.copyFrom (c, 0, x.data() + p, 512); else b.clear (c, 0, 512);
            proc.processBlock (b, midi);
            out.insert (out.end(), b.getReadPointer (0), b.getReadPointer (0) + 512);
        }
        return out;
    };
    auto a = render (false), c = render (true);
    CHECK (maxDiff (a, c) == 0.0);
    CHECK (rms (a, 12000) < rms (x, 12000));
    CHECK (proc.grDb.take() > 1.0f);

    // state round trip
    juce::MemoryBlock mb;
    proc.getStateInformation (mb);
    WCompProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (other.apvts.getRawParameterValue (wcomp::threshold)->load(), -30.0, 1e-4);
    CHECK_NEAR (other.apvts.getRawParameterValue (wcomp::ratio)->load(), 4.0, 0.02);

    // Mode Mono: the channels are compressed independently (a loud left leaves the quiet right alone)
    set (wcomp::mode, 1.0f);
    proc.prepareToPlay (48000.0, 512);
    auto loud = tone (100.0, 0.5, -6.0), quiet = tone (1000.0, 0.5, -40.0);
    juce::AudioBuffer<float> b (4, 512);
    juce::MidiBuffer midi;
    std::vector<float> right;
    for (size_t p = 0; p + 512 <= loud.size(); p += 512)
    {
        b.clear(); b.copyFrom (0, 0, loud.data() + p, 512); b.copyFrom (1, 0, quiet.data() + p, 512);
        proc.processBlock (b, midi);
        right.insert (right.end(), b.getReadPointer (1), b.getReadPointer (1) + 512);
    }
    const double makeup = wcomp::Compressor::makeupDb (-30.0, 4.0) * wcomp::Compressor::makeupScale (100.25);
    CHECK_NEAR (db (rms (right, 12000)) - db (rms (quiet, 12000)), makeup, 0.1);
}

static void testValueEntry()
{
    // a never-positive range (Threshold) reads "18" as -18; a bipolar one (Output) keeps the sign typed
    juce::Slider thr, out;
    thr.setRange (-48.0, 0.0, 0.1); out.setRange (-18.0, 18.0, 0.1);
    const auto fmt = [] (double v) { return juce::String (v, 1); };
    wavy::flat::applyFormat (thr, fmt, thr.getMaximum() <= 0.0);
    wavy::flat::applyFormat (out, fmt, out.getMaximum() <= 0.0);
    CHECK_NEAR (thr.getValueFromText ("18"), -18.0, 1e-9);
    CHECK_NEAR (thr.getValueFromText ("-18"), -18.0, 1e-9);
    CHECK_NEAR (out.getValueFromText ("5"), 5.0, 1e-9);
    CHECK_NEAR (out.getValueFromText ("-5"), -5.0, 1e-9);
}

static void testPresets()
{
    // every factory preset loads through the host's program list; Mix stays where the user left it
    WCompProcessor proc;
    const auto& list = wcomp::factoryPresets();
    CHECK (list.size() == 26);
    CHECK (proc.getNumPrograms() == 27);
    auto* mix = proc.apvts.getParameter (wcomp::mix);
    mix->setValueNotifyingHost (mix->convertTo0to1 (37.0f));
    for (int i = 0; i < (int) list.size(); ++i)
    {
        proc.setCurrentProgram (i + 1);
        for (const auto& v : list[(size_t) i].values)
        {
            if (std::string (v.id) == wcomp::mix) continue;
            auto* prm = proc.apvts.getParameter (v.id);
            CHECK (prm != nullptr);
            if (prm != nullptr) CHECK_NEAR (prm->convertFrom0to1 (prm->getValue()), v.value, 0.02 + 0.001 * std::abs (v.value));
        }
        CHECK_NEAR (proc.apvts.getRawParameterValue (wcomp::mix)->load(), 37.0, 0.05);
    }
}

// the needle laws against the filmed reference (MEASUREMENTS.md "WComp needle"; frames at about 60 Hz)
static void testNeedle()
{
    using namespace wcomp::needle;
    const double dt = 1.0 / 60.0;
    const double m6 = std::pow (10.0, -6.0 / 20.0), m12 = std::pow (10.0, -12.0 / 20.0), m18 = std::pow (10.0, -18.0 / 20.0),
                 m24 = std::pow (10.0, -24.0 / 20.0);
    CHECK (frame (1.0) == 70 && frame (0.0) == 0 && frame (2.0) == 70);
    CHECK_NEAR (angle (70), 59.0, 1e-6);                  // the filmed strip: whole degrees, -58 ... 59
    CHECK_NEAR (angle (0), -58.0, 1e-6);
    CHECK_NEAR (angle (35), 1.0, 1e-6);
    CHECK (frame (ioValue (m6)) == 61);                   // filmed: -6 dBFS 61, -24 dBFS 35, -48 dBFS 0
    CHECK (frame (ioValue (m24)) == 35);
    CHECK (frame (ioValue (std::pow (10.0, -48.0 / 20.0))) == 0);

    // GR: the deepest gain shows at once; the return is slower than the gain's (filmed after a 50 ms burst at -6 dBFS,
    // threshold -30, ratio 4: the gain back from frame 10 to 24, 39, 52, 66, 70 at 5, 15, 35, 65, 85 ms after its end, the
    // needle at 25, 50, 65 at 95, 225, 415 ms)
    Gr gr;
    CHECK (frame (gr.step (0.137, dt)) == 10);
    static const double gt[] = { 0.0, 0.005, 0.015, 0.035, 0.065, 0.085, 10.0 }, gv[] = { 10, 24, 39, 52, 66, 70, 70 };
    auto gain = [&] (double t)
    {
        int i = 0;
        while (t > gt[i + 1]) ++i;
        return (gv[i] + (gv[i + 1] - gv[i]) * (t - gt[i]) / (gt[i + 1] - gt[i])) / 70.0;
    };
    const double ms = 0.001;                               // 1 ms steps: the comparison is with the gain, not the frames
    int f95 = 0, f225 = 0, f415 = 0;
    for (int i = 1; i <= 1500; ++i)
    {
        const int f = frame (gr.step (gain (i * ms), ms));
        if (i == 95) f95 = f;
        if (i == 225) f225 = f;
        if (i == 415) f415 = f;
    }
    CHECK (std::abs (f95 - 25) <= 3);
    CHECK (std::abs (f225 - 50) <= 3);
    CHECK (std::abs (f415 - 65) <= 2);
    CHECK (frame (gr.x2) == 70);

    // In / Out: a 5 ms burst at -6 dBFS falls from its onset at about 38 frames a second (filmed: 39 at 0.58 s)
    Io io;
    io.step (m6, dt);
    for (int i = 0; i < 35; ++i) io.step (0.0, dt);
    CHECK (std::abs (frame (ioValue (io.shown)) - 39) <= 1);
    // a 300 ms burst: after it the hold from the onset shows (filmed: 48 at 0.345 s from the onset), then falls
    io.reset();
    for (int i = 0; i < 18; ++i) io.step (m6, dt);
    for (int i = 0; i < 3; ++i) io.step (0.0, 0.015);
    CHECK (std::abs (frame (ioValue (io.shown)) - 48) <= 1);
    // -12 for 1 s, then -18: the first drop shows at once; then -24: the second decays (filmed: 44, then 42 at 0.1 s)
    io.reset();
    for (int i = 0; i < 60; ++i) io.step (m12, dt);
    for (int i = 0; i < 60; ++i) io.step (m18, dt);
    CHECK (frame (ioValue (io.shown)) == 44);
    for (int i = 0; i < 6; ++i) io.step (m24, dt);
    CHECK (std::abs (frame (ioValue (io.shown)) - 42) <= 1);
    for (int i = 0; i < 90; ++i) io.step (m24, dt);
    CHECK (frame (ioValue (io.shown)) == 35);

    // the lamp: full at once, about 0.64 by 0.82 s, off after 0.833 s
    Lamp lamp;
    lamp.trigger();
    CHECK (lamp.step (dt) > 0.95);
    double v = 0.0;
    for (int i = 0; i < 48; ++i) v = lamp.step (dt);
    CHECK (v > 0.6 && v < 0.7);
    for (int i = 0; i < 2; ++i) v = lamp.step (dt);
    CHECK (v == 0.0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testLatency();
    testIdentity();
    testStatic();
    testBlockIndependence();
    testLinkAndLimiter();
    testMix();
    testSync();
    testAnalog();
    testProcessor();
    testValueEntry();
    testPresets();
    testNeedle();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
