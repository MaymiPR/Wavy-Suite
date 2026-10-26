// Unit tests for WBass Ride (Phase 2): the parameters as the reference's, the components' latency, the audio path
// until the riding DSP, the state, the editor.
// Run: WBassRideTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WBassRideProcessor.h"
#include "WBassRideEditor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

// the defaults, ranges and texts of the reference's VST3 parameters (RESEARCH.md, WBass Ride)
static void testParameters()
{
    WBassRideProcessor p;
    auto v = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto range = [&p] (const char* id) { return p.apvts.getParameter (id)->getNormalisableRange(); };
    CHECK_NEAR (v (wbassride::target), -14.0, 1e-6);
    CHECK_NEAR (v (wbassride::sensitivity), 50.0, 1e-6);
    CHECK_NEAR (v (wbassride::response), 0.0, 1e-6);
    CHECK_NEAR (v (wbassride::spill), -40.0, 1e-6);
    CHECK_NEAR (v (wbassride::artifacts), 50.0, 1e-6);
    CHECK_NEAR (v (wbassride::rangeMax), 12.0, 1e-6);
    CHECK_NEAR (v (wbassride::rangeMin), -12.0, 1e-6);
    CHECK_NEAR (v (wbassride::idle), 0.5, 1e-6);
    CHECK_NEAR (v (wbassride::output), 0.0, 1e-6);
    CHECK_NEAR (range (wbassride::target).start, -42.0, 1e-6);
    CHECK_NEAR (range (wbassride::target).end, 0.0, 1e-6);
    CHECK_NEAR (range (wbassride::spill).start, -50.0, 1e-6);
    CHECK_NEAR (range (wbassride::spill).end, -3.0, 1e-6);
    CHECK_NEAR (range (wbassride::rangeMax).start, -11.0, 1e-6);
    CHECK_NEAR (range (wbassride::rangeMin).end, 11.0, 1e-6);
    CHECK_NEAR (range (wbassride::idle).start, 0.02, 1e-6);
    CHECK_NEAR (range (wbassride::idle).end, 0.98, 1e-6);
    // the Idle arrow in dB: its place in the range (a preset with Idle 0.98 and Max 0 sits at -0.24 dB)
    CHECK_NEAR (wbassride::idleDb (0.5f, -12.0f, 12.0f), 0.0, 1e-6);
    CHECK_NEAR (wbassride::idleDb (0.98f, -12.0f, 0.0f), -0.24, 1e-5);
    CHECK (p.apvts.getParameter (wbassride::idle)->getText (0.5f, 16) == "0.0");
    CHECK (p.apvts.getParameter (wbassride::response)->getText (1.0f, 16) == "Fast");
    CHECK (wbassride::modeNames().size() == 4);
}

// the components' latency (measured: 42.00 ms, Live 5.00 ms at any rate) and the audio path: the input delayed by it,
// at the idle gain and Output
static void testLatencyAndPath()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int mode : { 0, 2 })
        {
            WBassRideProcessor p;
            auto* m = p.apvts.getParameter (wbassride::mode);
            m->setValueNotifyingHost (m->convertTo0to1 ((float) mode));
            p.setPlayConfigDetails (2, 2, fs, 256);
            p.prepareToPlay (fs, 256);
            const int expect = juce::roundToInt ((mode == 2 ? 5.0 : 42.0) * 0.001 * fs);
            CHECK (p.getLatencySamples() == expect);
            auto* o = p.apvts.getParameter (wbassride::output);
            o->setValueNotifyingHost (o->convertTo0to1 (-6.0f));
            juce::AudioBuffer<float> b (2, 256);
            juce::MidiBuffer midi;
            int found = -1, pos = 0;
            float peak = 0.0f;
            for (int blk = 0; blk < 40 && found < 0; ++blk, pos += 256)
            {
                b.clear();
                if (blk == 1) { b.setSample (0, 10, 1.0f); b.setSample (1, 10, 1.0f); }
                p.processBlock (b, midi);
                for (int i = 0; i < 256; ++i)
                    if (std::abs (b.getSample (0, i)) > 0.1f) { found = pos + i; peak = b.getSample (0, i); break; }
            }
            CHECK (found == 256 + 10 + expect);
            CHECK_NEAR (peak, juce::Decibels::decibelsToGain (-6.0), 1e-3);
        }
}

static void testStateAndEditor()
{
    WBassRideProcessor p;
    auto* t = p.apvts.getParameter (wbassride::target);
    t->setValueNotifyingHost (t->convertTo0to1 (-20.0f));
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    WBassRideProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK_NEAR (q.apvts.getRawParameterValue (wbassride::target)->load(), -20.0, 1e-4);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    CHECK (ed != nullptr);
    CHECK (ed->getWidth() > 0);
    CHECK (wbassride::ui::scaleY (12.0f) == 222.0f);
    CHECK (wbassride::ui::scaleY (-12.0f) == 432.0f);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testParameters();
    testLatencyAndPath();
    testStateAndEditor();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
