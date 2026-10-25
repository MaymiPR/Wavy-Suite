// Unit tests for WDelay (Phase 2: the parameters and the editor; the DSP comes in Phase 4). Run: WDelayTests (exit code
// 0 = all passed).
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WDelayProcessor.h"
#include "WDelayEditor.h"

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

// the editor's Ping Pong and Dual behaviour, as measured on the reference: clicks and drags on the display (editor pixels)
static void testDisplayClicks()
{
    WDelayProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);                                  // (off screen; hit-testing skips hidden components)
    auto set = [&p] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    auto get = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto tick = [&ed] { ((juce::Timer*) dynamic_cast<WDelayEditor*> (ed.get()))->timerCallback(); };   // its 60 Hz update (a private base)
    auto event = [] (juce::Component* c, juce::Point<float> local, juce::ModifierKeys mods)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), local, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 c, c, now, local, now, 1, false);
    };
    // a click, or a press, a drag up by `up` px in 1 px steps and the release
    auto press = [&] (float x, float y, int up)
    {
        const juce::Point<float> pt (x, y);
        auto* c = ed->getComponentAt (pt.roundToInt());
        if (c == nullptr) return;
        const auto local = c->getLocalPoint (ed.get(), pt);
        c->mouseDown (event (c, local, juce::ModifierKeys::leftButtonModifier));
        for (int k = 1; k <= up; ++k) c->mouseDrag (event (c, local - juce::Point<float> (0.0f, (float) k), juce::ModifierKeys::leftButtonModifier));
        c->mouseUp (event (c, local - juce::Point<float> (0.0f, (float) up), {}));
    };
    auto knobValue = [&] { auto* s = dynamic_cast<juce::Slider*> (ed->getComponentAt (204, 101)); return s != nullptr ? s->getValue() : -1.0; };

    // Ping Pong: the start box flips the start side at each click
    set (wdelay::pingPong, 1.0f); tick();
    press (403.0f, 108.0f, 0);
    CHECK (get (wdelay::pingPongStart) > 0.5f);
    press (403.0f, 108.0f, 0);
    CHECK (get (wdelay::pingPongStart) < 0.5f);
    // a note on the display steps with a vertical drag (40 px: one value), in Ping Pong's layout as in the plain one
    press (315.0f, 112.0f, 40);
    CHECK_NEAR (get (wdelay::delayNote), 12.0, 1.0e-3);
    set (wdelay::pingPong, 0.0f); tick();
    press (320.0f, 105.0f, 40);
    CHECK_NEAR (get (wdelay::delayNote), 13.0, 1.0e-3);
    // Dual: switched on, the right side starts from the left's time; a click on the right note puts the Delay knob on
    // the right side
    set (wdelay::dual, 1.0f); tick();
    CHECK_NEAR (get (wdelay::delayNoteR), 13.0, 1.0e-3);
    set (wdelay::delayNoteR, 15.0f); tick();
    CHECK_NEAR (knobValue(), 13.0, 1.0e-3);
    press (409.0f, 112.0f, 0);
    CHECK_NEAR (knobValue(), 15.0, 1.0e-3);
    // Ping Pong's layout shows the left side; the right side comes back when Ping Pong goes off
    set (wdelay::pingPong, 1.0f); tick();
    CHECK_NEAR (knobValue(), 13.0, 1.0e-3);
    set (wdelay::pingPong, 0.0f); tick();
    CHECK_NEAR (knobValue(), 15.0, 1.0e-3);
    press (315.0f, 112.0f, 0);
    CHECK_NEAR (knobValue(), 13.0, 1.0e-3);
    // in MS a drag on the right time sets the right side's time
    set (wdelay::sync, 2.0f); tick();
    const float r0 = get (wdelay::delayR);
    press (395.0f, 110.0f, 20);
    CHECK (get (wdelay::delayR) > r0 + 5.0f);
    CHECK_NEAR (knobValue(), get (wdelay::delayR), 1.0e-3);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testDelayTaper();
    testNotes();
    testTexts();
    testProcessor();
    testDisplayClicks();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
