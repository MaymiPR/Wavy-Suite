// Unit tests for WBass Rider: the parameters as the reference's, the components' latency, the audio path, the ride
// against the reference's measured behaviour (law, look-ahead, hold, glide, stereo mid), the state, the editor.
// Run: WBassRiderTests (exit code 0 = all passed).
#include <functional>
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
            auto* o = p.apvts.getParameter (wbassrider::output);
            o->setValueNotifyingHost (o->convertTo0to1 (-6.0f));
            p.setPlayConfigDetails (2, 2, fs, 256);
            p.prepareToPlay (fs, 256);
            const int expect = juce::roundToInt ((mode == 2 ? 5.0 : 42.0) * 0.001 * fs);
            CHECK (p.getLatencySamples() == expect);
            juce::AudioBuffer<float> b (2, 256);
            juce::MidiBuffer midi;
            int found = -1, pos = 0;
            float peak = 0.0f;
            for (int blk = 0; blk < 40 && found < 0; ++blk, pos += 256)
            {
                b.clear();
                // far under the detector's floor (-48.75 dBFS, measured): no note, the Rider at its idle value (0 dB)
                if (blk == 1) { b.setSample (0, 10, 0.001f); b.setSample (1, 10, 0.001f); }
                p.processBlock (b, midi);
                for (int i = 0; i < 256; ++i)
                    if (std::abs (b.getSample (0, i)) > 0.0001f) { found = pos + i; peak = b.getSample (0, i); break; }
            }
            CHECK (found == 256 + 10 + expect);
            CHECK_NEAR (peak, 0.001 * juce::Decibels::decibelsToGain (-6.0), 2e-6);
        }
}

// a processor at 48 kHz fed with blocks of 480 samples; `make` fills each block (left, right), the output is kept
static std::vector<float> runRide (WBassRiderProcessor& p, int blocks, std::function<void (int, float&, float&)> make, int outCh = 0)
{
    const double fs = 48000.0;
    p.setPlayConfigDetails (2, 2, fs, 480);
    p.prepareToPlay (fs, 480);
    juce::AudioBuffer<float> b (2, 480);
    juce::MidiBuffer midi;
    std::vector<float> out;
    for (int blk = 0; blk < blocks; ++blk)
    {
        for (int i = 0; i < 480; ++i) { float l = 0.0f, r = 0.0f; make (blk * 480 + i, l, r); b.setSample (0, i, l); b.setSample (1, i, r); }
        p.processBlock (b, midi);
        for (int i = 0; i < 480; ++i) out.push_back (b.getSample (outCh, i));
    }
    return out;
}

static double sineAt (int n, double f, double peakDb) { return std::pow (10.0, peakDb / 20.0) * std::sin (2.0 * juce::MathConstants<double>::pi * f * n / 48000.0); }

// the ride against the reference's measured law (MEASUREMENTS.md, WBass Rider Phase 3): gain = Target - E + 0.68 dB with
// E the weighted level (an 80 Hz sine reads 4.81 dB under its peak): an 80 Hz tone at -20 dBFS rms rides to +8.48 dB
// with the Energy meter at -21.8; when it stops the gain glides to the Idle value at 32 dB/s (back within 0.4 s)
static void testRideLaw()
{
    WBassRiderProcessor p;
    float energy = -100.0f;
    runRide (p, 100, [] (int n, float& l, float& r) { l = r = (float) sineAt (n, 80.0, -16.99); });
    energy = p.energyDb.take();
    CHECK_NEAR (p.rideDb.load(), 8.48, 0.1);
    CHECK_NEAR (energy, -21.8, 0.2);
    // (continue on the same processor: 0.4 s of silence)
    juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer midi;
    for (int blk = 0; blk < 25; ++blk) { b.clear(); p.processBlock (b, midi); }
    const float after250ms = p.rideDb.load();
    for (int blk = 0; blk < 15; ++blk) { b.clear(); p.processBlock (b, midi); }
    CHECK (after250ms < 8.0f && after250ms > 0.5f);                        // gliding (32 dB/s)
    CHECK_NEAR (p.rideDb.load(), 0.0, 0.05);
}

// the look-ahead: a legato step -30 -> -16 dBFS (1 kHz) at 1 s; the gain starts down 42 ms before the louder audio
// leaves the plug-in (measured on the reference), so the output never shows the step at the old gain
static void testRideLookAhead()
{
    WBassRiderProcessor p;
    const int step = 48000;
    auto y = runRide (p, 200, [step] (int n, float& l, float& r) { l = r = (float) sineAt (n, 1000.0, n < step ? -30.0 : -16.0); });
    const int lat = p.getLatencySamples();
    CHECK (lat == 2016);
    // the output's envelope just after the step reaches the output: about -16 + the new gain (+2.69), not -16 + 12
    float pk = 0.0f;
    for (int i = step + lat + 100; i < step + lat + 600; ++i) pk = std::max (pk, std::abs (y[(size_t) i]));
    CHECK_NEAR (juce::Decibels::gainToDecibels (pk), -16.0 + 2.69, 0.3);
}

// within a note the gain only goes down: a legato step down (-16 -> -30) keeps the gain of the louder part
static void testRideHold()
{
    WBassRiderProcessor p;
    runRide (p, 300, [] (int n, float& l, float& r) { l = r = (float) sineAt (n, 1000.0, n < 48000 ? -16.0 : -30.0); });
    CHECK_NEAR (p.rideDb.load(), 2.69, 0.1);
}

// stereo: one gain for both channels, from the mid: L at -6, R at -30 dBFS (in phase, 1 kHz) ride as their mid (-11.5)
static void testRideStereoMid()
{
    WBassRiderProcessor p;
    runRide (p, 100, [] (int n, float& l, float& r) { l = (float) sineAt (n, 1000.0, -6.0); r = (float) sineAt (n, 1000.0, -30.0); });
    CHECK_NEAR (p.rideDb.load(), -1.82, 0.15);
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
    testRideLaw();
    testRideLookAhead();
    testRideHold();
    testRideStereoMid();
    testStateAndEditor();
    testSelection();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
