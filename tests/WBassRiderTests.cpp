// Unit tests for WBass Rider (Phase 2): the parameters as the reference's, the components' latency, the audio path
// until the riding DSP, the state, the editor.
// Run: WBassRiderTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WBassRiderProcessor.h"
#include "WBassRiderEditor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

// the defaults, ranges and texts of the reference's VST3 parameters (RESEARCH.md, WBass Rider)
static void testParameters()
{
    WBassRiderProcessor p;
    auto v = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto range = [&p] (const char* id) { return p.apvts.getParameter (id)->getNormalisableRange(); };
    CHECK_NEAR (v (wbassrider::target), -14.0, 1e-6);
    CHECK_NEAR (v (wbassrider::sensitivity), 50.0, 1e-6);
    CHECK_NEAR (v (wbassrider::response), 0.0, 1e-6);
    CHECK_NEAR (v (wbassrider::spill), -40.0, 1e-6);
    CHECK_NEAR (v (wbassrider::artifacts), 50.0, 1e-6);
    CHECK_NEAR (v (wbassrider::rangeMax), 12.0, 1e-6);
    CHECK_NEAR (v (wbassrider::rangeMin), -12.0, 1e-6);
    CHECK_NEAR (v (wbassrider::idle), 0.5, 1e-6);
    CHECK_NEAR (v (wbassrider::output), 0.0, 1e-6);
    CHECK_NEAR (range (wbassrider::target).start, -42.0, 1e-6);
    CHECK_NEAR (range (wbassrider::target).end, 0.0, 1e-6);
    CHECK_NEAR (range (wbassrider::spill).start, -50.0, 1e-6);
    CHECK_NEAR (range (wbassrider::spill).end, -3.0, 1e-6);
    CHECK_NEAR (range (wbassrider::rangeMax).start, -11.0, 1e-6);
    CHECK_NEAR (range (wbassrider::rangeMin).end, 11.0, 1e-6);
    CHECK_NEAR (range (wbassrider::idle).start, 0.02, 1e-6);
    CHECK_NEAR (range (wbassrider::idle).end, 0.98, 1e-6);
    // the Idle arrow in dB: its place in the range (a preset with Idle 0.98 and Max 0 sits at -0.24 dB)
    CHECK_NEAR (wbassrider::idleDb (0.5f, -12.0f, 12.0f), 0.0, 1e-6);
    CHECK_NEAR (wbassrider::idleDb (0.98f, -12.0f, 0.0f), -0.24, 1e-5);
    CHECK (p.apvts.getParameter (wbassrider::idle)->getText (0.5f, 16) == "0.0");
    CHECK (p.apvts.getParameter (wbassrider::response)->getText (1.0f, 16) == "Fast");
    CHECK (wbassrider::modeNames().size() == 4);
}

// the components' latency (measured: 42.00 ms, Live 5.00 ms at any rate) and the audio path: the input delayed by it,
// at the idle gain and Output
static void testLatencyAndPath()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int mode : { 0, 2 })
        {
            WBassRiderProcessor p;
            auto* m = p.apvts.getParameter (wbassrider::mode);
            m->setValueNotifyingHost (m->convertTo0to1 ((float) mode));
            p.setPlayConfigDetails (2, 2, fs, 256);
            p.prepareToPlay (fs, 256);
            const int expect = juce::roundToInt ((mode == 2 ? 5.0 : 42.0) * 0.001 * fs);
            CHECK (p.getLatencySamples() == expect);
            auto* o = p.apvts.getParameter (wbassrider::output);
            o->setValueNotifyingHost (o->convertTo0to1 (-6.0f));
            juce::AudioBuffer<float> b (2, 256);
            juce::MidiBuffer midi;
            int found = -1, pos = 0;
            float peak = 0.0f;
            for (int blk = 0; blk < 40 && found < 0; ++blk, pos += 256)
            {
                b.clear();
                // under the Spill gate (-40 dBFS): no note, the Rider at its idle value (0 dB)
                if (blk == 1) { b.setSample (0, 10, 0.005f); b.setSample (1, 10, 0.005f); }
                p.processBlock (b, midi);
                for (int i = 0; i < 256; ++i)
                    if (std::abs (b.getSample (0, i)) > 0.0005f) { found = pos + i; peak = b.getSample (0, i); break; }
            }
            CHECK (found == 256 + 10 + expect);
            CHECK_NEAR (peak, 0.005 * juce::Decibels::decibelsToGain (-6.0), 1e-5);
        }
}

// the provisional riding model against the reference's films: an 80 Hz tone at -20 dBFS rms rides to +8.47 dB with the
// Energy meter at -21.9 dBFS; when it stops the Rider is back at its idle value within 0.4 s (the reference: 0.3 s)
static void testProvisionalRide()
{
    WBassRiderProcessor p;
    const double fs = 48000.0;
    p.setPlayConfigDetails (2, 2, fs, 480);
    p.prepareToPlay (fs, 480);
    juce::AudioBuffer<float> b (2, 480);
    juce::MidiBuffer midi;
    double ph = 0.0;
    float energy = -100.0f;
    for (int blk = 0; blk < 100; ++blk)                              // 1 s
    {
        for (int i = 0; i < 480; ++i, ph += 80.0 / fs)
            b.setSample (0, i, (float) (0.1 * std::sqrt (2.0) * std::sin (2.0 * juce::MathConstants<double>::pi * ph))), b.setSample (1, i, b.getSample (0, i));
        p.processBlock (b, midi);
        energy = p.energyDb.take();
    }
    CHECK_NEAR (p.rideDb.load(), 8.47, 0.4);
    CHECK_NEAR (energy, -21.9, 0.5);
    for (int blk = 0; blk < 40; ++blk) { b.clear(); p.processBlock (b, midi); }   // 0.4 s of silence
    CHECK_NEAR (p.rideDb.load(), 0.0, 0.05);
}

static void testStateAndEditor()
{
    WBassRiderProcessor p;
    auto* t = p.apvts.getParameter (wbassrider::target);
    t->setValueNotifyingHost (t->convertTo0to1 (-20.0f));
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    WBassRiderProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (q.apvts.getRawParameterValue (wbassrider::target)->load(), -20.0, 1e-4);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    CHECK (ed != nullptr);
    CHECK (ed->getWidth() > 0);
    CHECK (wbassrider::ui::scaleY (12.0f) == 222.0f);
    CHECK (wbassrider::ui::scaleY (-12.0f) == 432.0f);
}

// the selection rules measured on the reference (click, lasso group, Rider / Idle in blue, member drags blocked)
static void testSelection()
{
    using namespace wbassrider::ui;
    juce::Component a, b, rider;
    Marks m;
    m.blueAlone = { &rider };
    CHECK (m.click (&a));
    CHECK (m.of (&a) == Mark::Selected);
    CHECK (m.of (&b) == Mark::None);
    CHECK (m.click (&rider));
    CHECK (m.of (&rider) == Mark::Member);                          // the Rider alone: blue
    m.group = { &rider, &a, &b };
    m.selected = &rider;
    CHECK (m.of (&rider) == Mark::Selected);                        // in a group the first is yellow
    CHECK (m.of (&b) == Mark::Member);
    CHECK (! m.click (&b));                                          // a member: its drag moves nothing
    CHECK (m.of (&b) == Mark::Selected && m.of (&rider) == Mark::Member);
    m.hover = &b;
    CHECK (m.showsValue (&a) && m.showsValue (&b));                 // the primary under the mouse: every member's value
    juce::Component other;
    CHECK (m.click (&other));                                        // outside the group: the group goes
    CHECK (m.group.empty() && m.of (&a) == Mark::None);
    // the host sees Idle over a fixed +-12 dB, whatever the range (as the reference's text)
    WBassRiderProcessor p;
    auto* mx = p.apvts.getParameter (wbassrider::rangeMax);
    mx->setValueNotifyingHost (mx->convertTo0to1 (6.0f));
    CHECK (p.apvts.getParameter (wbassrider::idle)->getText (0.5f, 16) == "0.0");
    CHECK_NEAR (p.idleDb(), -3.0, 1e-4);                             // the arrow itself: the middle of -12 .. 6
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testParameters();
    testLatencyAndPath();
    testProvisionalRide();
    testStateAndEditor();
    testSelection();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
