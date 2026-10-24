// Unit tests for WDelay (Phase 2: the parameters and the editor; the DSP comes in Phase 4). Run: WDelayTests (exit code
// 0 = all passed).
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WDelayProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; std::printf ("FAIL %s:%d  %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, va, vb, (double) (tol)); } } while (0)

// the Delay taper against values read from the reference (build/hdelay_tapers.json, normalised 0, 0.05, ... 1)
static void testDelayTaper()
{
    const float ref[] = { 1, 51, 101, 151, 201, 251, 301, 351, 401, 451, 501, 636, 815, 1024, 1265, 1540, 1852, 2201, 2591, 3023, 3500 };
    for (int i = 0; i <= 20; ++i) CHECK_NEAR (wdelay::delayFromNorm ((float) i / 20.0f), ref[i], 0.5);
    for (float ms : { 1.0f, 7.0f, 375.0f, 501.0f, 502.0f, 777.0f, 1999.0f, 3499.0f, 3500.0f })
        CHECK_NEAR (wdelay::delayFromNorm (wdelay::delayToNorm (ms)), ms, 1.0e-3);
    float last = 0.0f;
    bool rising = true;
    for (int i = 0; i <= 1000; ++i)
    {
        const float v = wdelay::delayFromNorm ((float) i / 1000.0f);
        rising = rising && v > last;
        last = v;
    }
    CHECK (rising);
}

static void testNotes()
{
    CHECK_NEAR (wdelay::noteBeats (11) * 60000.0 / 120.0, 375.0, 1.0e-9);         // 1/8D at 120 BPM: the default 375 ms
    CHECK_NEAR (wdelay::noteBeats (19) * 60000.0 / 120.0, 4000.0, 1.0e-9);        // 2 bars
    CHECK (wdelay::nearestNote (375.0, 120.0) == 11);
    CHECK (wdelay::nearestNote (500.0, 120.0) == 13);                                  // a quarter
    CHECK (wdelay::nearestNote (0.5, 120.0) == 0);
    CHECK (wdelay::noteNames().size() == 20);
}

// the parameters' texts as the reference shows them
static void testTexts()
{
    CHECK (wdelay::freqText (19.0f, true) == "Off");
    CHECK (wdelay::freqText (154.0f, true) == "154Hz");
    CHECK (wdelay::freqText (2490.0f, false) == "2.49kHz");
    CHECK (wdelay::freqText (20138.77f, false) == "Off");
    CHECK (wdelay::rateText (0.1f) == "0.10 Hz");
    CHECK (wdelay::rateText (24.5f) == "24.5 Hz");
    CHECK (wdelay::rateText (6000.0f) == "6.00k Hz");
}

// defaults as a fresh reference instance; the audio passes through unchanged until the DSP is in (Phase 4)
static void testProcessor()
{
    WDelayProcessor p;
    auto v = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    CHECK_NEAR (v (wdelay::delay), 375.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::delayNote), 11.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::sync), 1.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::feedback), 60.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::mix), 100.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::analog), 2.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::hipass), 19.0, 1.0e-3);
    CHECK_NEAR (v (wdelay::lopass), 20138.77, 0.05);
    CHECK (p.getLatencySamples() == 0);
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> b (2, 512);
    for (int i = 0; i < 512; ++i) { b.setSample (0, i, std::sin ((float) i * 0.1f)); b.setSample (1, i, 0.5f); }
    juce::MidiBuffer m;
    p.processBlock (b, m);
    CHECK_NEAR (b.getSample (0, 100), std::sin (10.0f), 1.0e-6);
    CHECK_NEAR (b.getSample (1, 100), 0.5f, 1.0e-6);
    // the state round trip
    p.apvts.getParameter (wdelay::feedback)->setValueNotifyingHost (p.apvts.getParameter (wdelay::feedback)->convertTo0to1 (137.0f));
    juce::MemoryBlock st;
    p.getStateInformation (st);
    WDelayProcessor q;
    q.setStateInformation (st.getData(), (int) st.getSize());
    CHECK_NEAR (q.apvts.getRawParameterValue (wdelay::feedback)->load(), 137.0, 1.0e-3);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testDelayTaper();
    testNotes();
    testTexts();
    testProcessor();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
