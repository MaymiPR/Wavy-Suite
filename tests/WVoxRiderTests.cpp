// Unit tests for WVox Rider (Phases 2, 4): the parameters as the reference's, the buses (a side-chain), no latency, the
// audio path, Automation (Read plays the host's Rider Fader, Write sends the ride), the editor (Studio / Live layouts,
// the Music knob's disabled look without a side-chain), the factory presets, and the riding against values measured on the reference
// (MEASUREMENTS.md, WVox Rider Phase 3 / 4).
// Run: WVoxRiderTests (exit code 0 = all passed).
#include <functional>
#include <vector>
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WVoxRiderProcessor.h"
#include "WVoxRiderEditor.h"
#include "WVoxRiderRide.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

static void set (WVoxRiderProcessor& p, const char* id, float v)
{
    auto* q = p.apvts.getParameter (id);
    q->setValueNotifyingHost (q->convertTo0to1 (v));
}

// the defaults, ranges and texts of the reference's VST3 parameters (RESEARCH.md, WVox Rider)
static void testParameters()
{
    WVoxRiderProcessor p;
    auto v = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto range = [&p] (const char* id) { return p.apvts.getParameter (id)->getNormalisableRange(); };
    CHECK_NEAR (v (wvoxrider::target), -21.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::vocalSensitivity), 0.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::musicSensitivity), 0.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::rangeMax), 6.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::rangeMin), -6.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::idle), 0.5, 1e-6);
    CHECK_NEAR (v (wvoxrider::rider), 0.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::output), 0.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::attack), 0.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::spill), -48.0, 1e-6);
    CHECK_NEAR (v (wvoxrider::automation), 0.0, 1e-6);
    CHECK_NEAR (range (wvoxrider::target).start, -42.0, 1e-6);
    CHECK_NEAR (range (wvoxrider::musicSensitivity).start, -12.1, 1e-5);
    CHECK_NEAR (range (wvoxrider::spill).start, -48.0, 1e-6);
    CHECK_NEAR (range (wvoxrider::spill).end, -12.0, 1e-6);
    CHECK_NEAR (range (wvoxrider::rangeMax).start, -11.0, 1e-6);
    CHECK_NEAR (range (wvoxrider::rangeMin).end, 11.0, 1e-6);
    CHECK (p.apvts.getParameter (wvoxrider::musicSensitivity)->getText (0.0f, 16) == "Off");
    CHECK (p.apvts.getParameter (wvoxrider::idle)->getText (0.5f, 16) == "0.0");
    CHECK (p.apvts.getParameter (wvoxrider::attack)->getText (1.0f, 16) == "Fast");
    CHECK (! p.apvts.getParameter (wvoxrider::automation)->isAutomatable());
    CHECK (wvoxrider::modeNames().size() == 4);
}

// a main input and output (mono or stereo) and a side-chain input; no latency
static void testBuses()
{
    WVoxRiderProcessor p;
    CHECK (p.getBusCount (true) == 2);
    CHECK (p.getLatencySamples() == 0);
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    CHECK (p.checkBusesLayoutSupported (layout));
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::mono();
    CHECK (p.checkBusesLayoutSupported (layout));
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (! p.checkBusesLayoutSupported (layout));
}

// the audio path: no delay; Automation Read applies the host's Rider Fader (and Output) to the input
static void testPathAndRead()
{
    WVoxRiderProcessor p;
    set (p, wvoxrider::automation, (float) wvoxrider::Read);
    set (p, wvoxrider::rider, -6.0f);
    set (p, wvoxrider::output, 3.0f);
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> b (2, 256);
    juce::MidiBuffer midi;
    double last = 0.0;
    for (int blk = 0; blk < 40; ++blk)
    {
        for (int i = 0; i < 256; ++i) { const float x = 0.1f * (float) std::sin (0.05 * (blk * 256 + i)); b.setSample (0, i, x); b.setSample (1, i, x); }
        p.processBlock (b, midi);
        last = b.getSample (0, 200) / (0.1 * std::sin (0.05 * (blk * 256 + 200)));
    }
    CHECK_NEAR (juce::Decibels::gainToDecibels (std::abs (last)), -3.0, 0.05);
    CHECK (last > 0.0);                                             // no delay: the output in phase with the input
}

// Automation Write: the processor sends the ride to the host's Rider Fader
static void testWrite()
{
    WVoxRiderProcessor p;
    set (p, wvoxrider::automation, (float) wvoxrider::Write);
    p.rideDb.store (4.2f);
    ((juce::Timer&) p).timerCallback();                              // (the processor's private timer)
    CHECK_NEAR (p.get (wvoxrider::rider), 4.2, 0.06);
}

// the editor: its size, Spill only on Live, the knobs moving with the layout, Music disabled without a side-chain
static void testEditor()
{
    WVoxRiderProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    CHECK (ed != nullptr);
    if (ed == nullptr) return;
    CHECK (ed->getWidth() == WVoxRiderEditor::W && ed->getHeight() == WVoxRiderEditor::H);
    auto find = [&ed] (const char* type, int nth) -> juce::Component*
    {
        int k = 0;
        std::function<juce::Component* (juce::Component&)> visit = [&] (juce::Component& c) -> juce::Component*
        {
            for (auto* ch : c.getChildren())
            {
                if (juce::String (typeid (*ch).name()).contains (type) && k++ == nth) return ch;
                if (auto* r = visit (*ch)) return r;
            }
            return nullptr;
        };
        return visit (*ed);
    };
    auto* vocal = find ("Knob", 0);
    auto* spill = find ("Knob", 2);
    CHECK (vocal != nullptr && spill != nullptr);
    if (vocal == nullptr || spill == nullptr) return;
    CHECK (! spill->isVisible());
    CHECK (vocal->getX() == 83);
    set (p, wvoxrider::mode, 2.0f);                                  // Live Stereo
    ((juce::Timer&) static_cast<wavy::ren::Editor&> (*ed)).timerCallback();   // (the editor's private timer: tick)
    CHECK (spill->isVisible());
    CHECK (vocal->getX() == 133);
    auto* music = dynamic_cast<wvoxrider::ui::Knob*> (find ("Knob", 1));
    CHECK (music != nullptr && music->disabledLook);                // no side-chain fed
    // a snapshot paints (the models render)
    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    CHECK (img.getWidth() == WVoxRiderEditor::W);
}

// the riding (wvoxrider::Ride) on 1 kHz sines at 48 kHz, against the reference's measured numbers
namespace ridetest
{
constexpr double fs = 48000.0;
// levels (dBFS rms; < -200 = silence) for durations (s); the gain in dB per sample
static std::vector<double> ride (const std::vector<std::pair<double, double>>& parts, const wvoxrider::RideSettings& st)
{
    wvoxrider::Ride r; r.prepare (fs);
    std::vector<double> g; double ph = 0.0;
    for (auto [lev, secs] : parts)
    {
        const double a = lev < -200.0 ? 0.0 : std::sqrt (2.0) * std::pow (10.0, lev / 20.0);
        for (int i = 0; i < (int) (secs * fs); ++i, ph += 2.0 * juce::MathConstants<double>::pi * 1000.0 / fs)
            g.push_back (r.push (a * std::sin (ph), st));
    }
    return g;
}
static double at (const std::vector<double>& g, double t) { return g[(size_t) (t * fs)]; }
// the variable part of the gain, v = gain - 1/2 gMin, in dB
static double vdb (double gdb, double rangeMin) { return 20.0 * std::log10 (std::pow (10.0, gdb / 20.0) - 0.5 * std::pow (10.0, rangeMin / 20.0)); }
}

static void testRiding()
{
    using namespace ridetest;
    wvoxrider::RideSettings st;
    // it starts at 1/2 gMin + 1 (the reference: +1.94 dB at Range -6 .. +6)
    auto g = ride ({ { -300.0, 0.1 } }, st);
    const double halfMin = 0.5 * std::pow (10.0, -6.0 / 20.0);
    CHECK_NEAR (g[0], 20.0 * std::log10 (halfMin + 1.0), 1.0e-6);
    // the law on a steady tone: 1/2 gMin + 0.738 Target / level (Phase 3, every setting within 0.1 dB)
    g = ride ({ { -27.0, 3.0 } }, st);
    CHECK_NEAR (at (g, 2.9), 20.0 * std::log10 (halfMin + 0.738 * std::pow (10.0, 6.0 / 20.0)), 0.15);
    // the rise after a drop goes only as fast as the detector passes samples: 12 dB down, v rises 2.25 dB in the
    // first 0.5 s; 4 dB down, 2.14 dB in 0.3 s (the reference, Range -12 .. +12, from -15 dBFS)
    st.rangeMin = -12.0; st.rangeMax = 12.0;
    g = ride ({ { -15.0, 3.0 }, { -27.0, 1.0 } }, st);
    CHECK_NEAR (vdb (at (g, 3.5), -12.0) - vdb (at (g, 3.0), -12.0), 2.25, 0.1);
    g = ride ({ { -15.0, 3.0 }, { -19.0, 1.0 } }, st);
    CHECK_NEAR (vdb (at (g, 3.3), -12.0) - vdb (at (g, 3.0), -12.0), 2.14, 0.1);
    // when the voice stops: held for 500 ms (Slow) / 50 ms (Fast), then v glides 7.21 dB/s to the Idle value, exactly
    // (here down, to -4 dB)
    st = {}; st.idleDb = -4.0;
    g = ride ({ { -24.0, 2.0 }, { -300.0, 2.0 } }, st);
    CHECK_NEAR (at (g, 2.45), at (g, 2.0), 0.001);
    CHECK_NEAR ((vdb (at (g, 2.6), -6.0) - vdb (at (g, 2.7), -6.0)) / 0.1, 7.21, 0.05);
    CHECK_NEAR (at (g, 3.95), -4.0, 1.0e-6);
    st.fast = true;
    g = ride ({ { -24.0, 2.0 }, { -300.0, 1.0 } }, st);
    CHECK_NEAR (at (g, 2.04), at (g, 2.0), 0.001);
    CHECK (at (g, 2.0) - at (g, 2.1) > 0.2);
    // Spill (Live) adds to the detector's threshold: just after a drop from -18 to -30 dBFS at Spill -30, nothing is
    // ridden for 0.4 s (the reference: -3.80 dB throughout); in Studio the gain rises 1.43 dB in that time
    st = {}; st.rangeMin = -12.0; st.rangeMax = 12.0; st.spill = -30.0; st.live = true;
    g = ride ({ { -18.0, 3.0 }, { -30.0, 1.0 } }, st);
    CHECK_NEAR (at (g, 3.4), at (g, 3.0), 0.02);
    st.live = false;
    g = ride ({ { -18.0, 3.0 }, { -30.0, 1.0 } }, st);
    CHECK_NEAR (at (g, 3.4) - at (g, 3.0), 1.43, 0.1);
    // Spill at its minimum (-48) is off
    st.live = true; st.spill = -48.0;
    auto h = ride ({ { -18.0, 3.0 }, { -30.0, 1.0 } }, st);
    CHECK_NEAR (at (h, 3.4), at (g, 3.4), 1.0e-9);
}

// the factory presets: the reference's 13, with our names; they set their values (Attack among them), and an Idle of
// 0 or 1 (outside the arrow's 0.02 .. 0.98) is kept exactly while the arrow stays put
static void testPresets()
{
    WVoxRiderProcessor p;
    CHECK (p.getNumPrograms() == 14);
    CHECK (p.getProgramName (1) == "Essentials: Full Reset");
    p.setCurrentProgram (12);                                         // "Ceiling Ride": Target -42, Range -12 .. 0, Fast, Idle 1
    CHECK (p.getProgramName (12) == "Essentials: Ceiling Ride");
    CHECK_NEAR (p.apvts.getRawParameterValue (wvoxrider::target)->load(), -42.0, 0.05);
    CHECK_NEAR (p.apvts.getRawParameterValue (wvoxrider::rangeMin)->load(), -12.0, 0.05);
    CHECK_NEAR (p.apvts.getRawParameterValue (wvoxrider::rangeMax)->load(), 0.0, 0.05);
    CHECK_NEAR (p.apvts.getRawParameterValue (wvoxrider::vocalSensitivity)->load(), -12.0, 0.05);
    CHECK (p.apvts.getRawParameterValue (wvoxrider::attack)->load() > 0.5f);
    CHECK_NEAR (p.idleDb(), 0.0, 1.0e-4);                             // Idle 1: at Max exactly (the arrow alone: -0.24)
    set (p, wvoxrider::idle, 0.5f);                                   // the arrow moved: the parameter's value again
    CHECK_NEAR (p.idleDb(), -6.0, 0.01);
    p.setCurrentProgram (13);                                         // "Open Up": Range -12 .. +12, Idle 0
    CHECK_NEAR (p.idleDb(), -12.0, 1.0e-4);
    p.setCurrentProgram (1);                                          // "Full Reset" also sets Automation Off
    CHECK_NEAR (p.apvts.getRawParameterValue (wvoxrider::target)->load(), -21.0, 0.05);
    CHECK (p.automationMode() == wvoxrider::Off);
    CHECK_NEAR (p.idleDb(), 0.0, 0.01);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testParameters();
    testBuses();
    testPathAndRead();
    testWrite();
    testEditor();
    testRiding();
    testPresets();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
