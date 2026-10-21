// Unit tests for WBassX: the DSP (WBassXDsp.h) and the plug-in processor. Run: WBassXTests (exit code 0 = all passed).
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WBassXDsp.h"
#include "WBassXProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

static const double kPi = 3.14159265358979323846;

static std::vector<float> tone (double f, double secs, double db, double fs = 48000.0)
{
    std::vector<float> x ((size_t) (fs * secs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (std::pow (10.0, db / 20.0) * std::sin (2.0 * kPi * f * (double) i / fs));
    return x;
}

struct Settings
{
    double input = 0, freq = 80, decay = -15, response = 20, ratio = 1, orig = 0, harm = -100;
    int highpass = 1, monitor = 0;
};

static void apply (wbassx::Processor& p, const Settings& s)
{
    p.setInputDb (s.input); p.setFreq (s.freq); p.setDecayDb (s.decay); p.setResponseMs (s.response); p.setRatio (s.ratio);
    p.setOrigBassDb (s.orig); p.setHarmonicsDb (s.harm); p.setHighPass (s.highpass); p.setMonitor (s.monitor);
}

static std::vector<float> run (const std::vector<float>& x, Settings s, int block = 512, double fs = 48000.0, double fixedGain = -1.0)
{
    wbassx::Processor p;
    apply (p, s);
    p.fixedGain = fixedGain;
    p.prepare (fs, 1);
    std::vector<float> y (x);
    for (size_t i = 0; i < y.size(); i += (size_t) block)
    {
        float* ch[1] = { y.data() + i };
        p.process (ch, 1, (int) std::min ((size_t) block, y.size() - i));
    }
    return y;
}

static std::complex<double> component (const std::vector<float>& y, double f, double secs = 0.5, double fs = 48000.0)
{
    const size_t n = (size_t) (std::round (std::floor (secs * f) * fs / f));
    std::complex<double> acc;
    for (size_t i = y.size() - n; i < y.size(); ++i) acc += (double) y[i] * std::polar (1.0, -2.0 * kPi * f * (double) i / fs);
    return 2.0 * acc / (double) n;
}

static double db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }
static double rmsDiffDb (const std::vector<float>& a, const std::vector<float>& b, size_t skip = 4800)
{
    double e = 0, r = 0;
    for (size_t i = skip; i < a.size(); ++i) { e += ((double) a[i] - b[i]) * ((double) a[i] - b[i]); r += (double) b[i] * b[i]; }
    return 10.0 * std::log10 (std::max (e, 1.0e-40) / std::max (r, 1.0e-40));
}

static void testOriginalPath()
{
    // Harmonics off: HP3(F) + OrigBass LP3(F); at 0 dB a flat all-pass
    Settings s;
    for (double f : { 20.0, 80.0, 300.0, 1000.0 })
    {
        auto x = tone (f, 1.0, -20);
        s.orig = 0;    CHECK_NEAR (db (std::abs (component (run (x, s), f))), -20.0, 0.01);
        s.orig = -100;
        CHECK_NEAR (db (std::abs (component (run (x, s), f))), -20.0 - 10.0 * std::log10 (1.0 + std::pow (80.0 / f, 6.0)), 0.05);
        s.orig = 0;
    }
    s.orig = 12; CHECK_NEAR (db (std::abs (component (run (tone (20, 1.0, -30), s), 20))), -30.0 + 12.0, 0.05);
    // Original Bass monitor: LP3(F) x, not changed by the fader
    Settings m; m.monitor = 2;
    for (double og : { -100.0, -6.0, 12.0 })
    {
        m.orig = og;
        CHECK_NEAR (db (std::abs (component (run (tone (80, 1.0, -20), m), 80))), -23.01, 0.05);
    }
}

static void testInput()
{
    Settings s;
    s.input = -6; CHECK_NEAR (db (std::abs (component (run (tone (1000, 0.5, -20), s), 1000))), -26.0, 0.01);
    s.input = 6;  CHECK_NEAR (db (std::abs (component (run (tone (1000, 0.5, -20), s), 1000))), -20.0, 0.01);   // stops at 0 dB
}

static void testHarmonicsFader()
{
    auto x = tone (50, 1.5, -20);
    Settings off; auto o = run (x, off);
    Settings g0; g0.monitor = 1; auto gen = run (x, g0);        // pre-fader generated part
    Settings h; h.harm = 6;
    auto a = run (x, h);
    // Audio = original + Harmonics * G
    std::vector<float> pred (a.size());
    for (size_t i = 0; i < a.size(); ++i) pred[i] = (float) (o[i] + std::pow (10.0, 6.0 / 20.0) * gen[i]);
    CHECK (rmsDiffDb (a, pred) < -100.0);
    // the monitor ignores the fader; the fader stops at +10.96 dB
    Settings g6 = g0; g6.harm = 6; CHECK (rmsDiffDb (run (x, g6), gen) < -200.0);
    Settings h11; h11.harm = 11; Settings h12; h12.harm = 12;
    CHECK (rmsDiffDb (run (x, h11), run (x, h12)) < -200.0);
    // there is a generated part, below Freq, with a 2nd harmonic
    CHECK (std::abs (component (gen, 50)) > 0.01);
    CHECK (std::abs (component (gen, 100)) > 1.0e-4);
}

static void testDecay()
{
    // Decay sets the even-harmonic product (about 1 dB per dB at low level); the level control does not see it
    Settings s; s.monitor = 1;
    auto x = tone (40, 1.5, -40);
    s.decay = -15; auto a = run (x, s);
    s.decay = -9;  auto b = run (x, s);
    CHECK_NEAR (db (std::abs (component (b, 80))) - db (std::abs (component (a, 80))), 6.0, 0.3);
    CHECK_NEAR (db (std::abs (component (b, 40))) - db (std::abs (component (a, 40))), 0.0, 0.05);
}

static void testHighPass()
{
    // the high-pass acts on the generated part only: at 20 Hz the 24 dB/oct setting is far below the 6 dB/oct one
    Settings s; s.monitor = 1;
    auto x = tone (20, 1.5, -30);
    s.highpass = 0; const double h0 = db (std::abs (component (run (x, s), 20)));
    s.highpass = 2; const double h2 = db (std::abs (component (run (x, s), 20)));
    CHECK (h0 - h2 > 20.0);
}

static void testClip()
{
    std::vector<float> x = tone (1000, 0.5, 0);
    for (auto& v : x) v *= 2.0f;
    auto y = run (x, Settings {});
    float mx = 0; for (auto v : y) mx = std::max (mx, std::abs (v));
    CHECK_NEAR (mx, 1.0, 1e-7);
}

static void testBlocksAndChannels()
{
    auto x = tone (55, 1.0, -12);
    Settings s; s.harm = 0; s.ratio = 2;
    CHECK (rmsDiffDb (run (x, s, 37), run (x, s, 512)) < -200.0);         // no block-size dependence
    // channels are processed on their own: a left-only input leaves the right output silent
    wbassx::Processor p; apply (p, s); p.prepare (48000.0, 2);
    std::vector<float> l (x), r (x.size(), 0.0f);
    float* ch[2] = { l.data(), r.data() };
    p.process (ch, 2, (int) x.size());
    double er = 0; for (auto v : r) er += (double) v * v;
    CHECK (er == 0.0);
    // the level control runs on its own 48 kHz clock: the generated part at 96 kHz stays close
    Settings m; m.monitor = 1;
    const double g48 = db (std::abs (component (run (tone (50, 1.5, -20), m), 50)));
    const double g96 = db (std::abs (component (run (tone (50, 1.5, -20, 96000.0), m, 512, 96000.0), 50, 0.5, 96000.0)));
    CHECK_NEAR (g96, g48, 0.5);
}

static void testProcessor()
{
    WBassXProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 512; ++i) { buf.setSample (0, i, 0.1f * std::sin ((float) i * 0.01f)); buf.setSample (1, i, 0.0f); }
    proc.processBlock (buf, midi);
    CHECK (std::isfinite (buf.getSample (0, 100)));
    // state round trip
    auto* fp = proc.apvts.getParameter (wbassx::freq);
    fp->setValueNotifyingHost (fp->convertTo0to1 (120.0f));
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    WBassXProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (other.apvts.getRawParameterValue (wbassx::freq)->load(), 120.0, 1e-3);
    // factory presets: 22 (from the reference's set), they set their values and leave Input alone
    CHECK (proc.getNumPrograms() == 23);
    auto* in = proc.apvts.getParameter (wbassx::input);
    in->setValueNotifyingHost (in->convertTo0to1 (-6.0f));
    proc.setCurrentProgram (2);                                       // "PA System"
    CHECK (proc.getProgramName (2) == "Essentials: PA System");
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbassx::input)->load(), -6.0, 0.05);
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbassx::ratio)->load(), 2.0, 0.01);
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbassx::harmonics)->load(), -15.0, 0.05);
    proc.setCurrentProgram (0);                                       // Default
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbassx::input)->load(), -6.0, 0.05);
    CHECK_NEAR (proc.apvts.getRawParameterValue (wbassx::harmonics)->load(), -100.0, 0.05);
}

static void testAgainstReference()
{
    // the identified structure against the reference's own measurements (MEASUREMENTS.md "WBassX model"), with the
    // level control held at its cap: the generated part at rest (impulse responses of MaxxBass at 1e-4, FFT magnitude)
    struct Row { double freq; int hp; std::vector<std::pair<double, double>> mag; };
    const Row rows[] = {
        { 80, 1, { { 20, -12.52 }, { 40, 0.63 }, { 60, 6.2 }, { 80, 7.1 }, { 120, 1.41 }, { 240, -15.56 }, { 640, -41.07 } } },
        { 80, 0, { { 20, 10.13 }, { 40, 12.83 }, { 80, 10.1 }, { 160, -5.0 }, { 640, -41.07 } } },
        { 80, 2, { { 20, -27.36 }, { 40, -4.5 }, { 80, 7.13 }, { 160, -5.22 } } },
        { 256, 1, { { 20, -30.88 }, { 60, -10.29 }, { 160, 5.92 }, { 240, 9.09 }, { 640, -9.06 } } } };
    for (const auto& r : rows)
        for (const auto& [f, ref] : r.mag)
        {
            Settings s; s.monitor = 1; s.freq = r.freq; s.highpass = r.hp;
            const auto y = run (tone (f, 1.5, -100), s, 512, 48000.0, 1.0);
            CHECK_NEAR (db (std::abs (component (y, f))) + 100.0, ref, 0.15);
        }
    // the same at 44.1 and 96 kHz: the reference designs its filters at the host rate (its K changes by under 0.3 %)
    for (double sr : { 44100.0, 96000.0 })
        for (const auto& [f, ref] : rows[0].mag)
        {
            Settings s; s.monitor = 1;
            const auto y = run (tone (f, 1.5, -100, sr), s, 512, sr, 1.0);
            CHECK_NEAR (db (std::abs (component (y, f, 0.5, sr))) + 100.0, ref, 0.15);
        }
    // the Decay feedback: a 50 Hz tone at -30 dBFS (Freq 80, gain at its cap, -0.04 dB as measured at Ratio 4), 2nd and
    // 3rd harmonics re input as measured on the reference
    const struct { double decay, h2, h3; } fb[] = { { -15, -22.2, -53.8 }, { -9, -16.2, -42.2 } };
    for (const auto& c : fb)
    {
        Settings s; s.monitor = 1; s.decay = c.decay;
        const auto y = run (tone (50, 1.5, -30), s, 512, 48000.0, std::pow (10.0, -0.04 / 20.0));
        CHECK_NEAR (db (std::abs (component (y, 100))) + 30.0, c.h2, 0.3);
        CHECK_NEAR (db (std::abs (component (y, 150))) + 30.0, c.h3, 0.6);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testOriginalPath();
    testInput();
    testHarmonicsFader();
    testDecay();
    testHighPass();
    testClip();
    testAgainstReference();
    testBlocksAndChannels();
    testProcessor();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
