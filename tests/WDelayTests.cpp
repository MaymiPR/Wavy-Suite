// Unit tests for WDelay: the parameters, the editor's display, the DSP against the laws measured on the reference.
// Run: WDelayTests (exit code 0 = all passed).
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
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
    // the DSP takes Rate as shown: three significant digits, at least 0.01 (measured)
    CHECK (std::abs (wdelay::atResolution (wdelay::rate, 3074.0f) - 3070.0f) < 1e-3f);
    CHECK (std::abs (wdelay::atResolution (wdelay::rate, 281.7f) - 282.0f) < 1e-4f);
    CHECK (std::abs (wdelay::atResolution (wdelay::rate, 47.84f) - 47.8f) < 1e-4f);
    CHECK (std::abs (wdelay::atResolution (wdelay::rate, 0.537f) - 0.54f) < 1e-5f);
}

// defaults as a fresh reference instance; an impulse through the processor comes back after 1/8D at 120 BPM
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
    auto set = [&p] (const char* id, float x) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (x)); };
    set (wdelay::analog, 0.0f);
    set (wdelay::feedback, 0.0f);
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    std::vector<float> out[2];
    juce::MidiBuffer m;
    for (int blk = 0; blk < 40; ++blk)
    {
        juce::AudioBuffer<float> b (2, 512);
        b.clear();
        if (blk == 0) { b.setSample (0, 100, 0.001f); b.setSample (1, 100, 0.001f); }
        p.processBlock (b, m);
        for (int c = 0; c < 2; ++c) out[c].insert (out[c].end(), b.getReadPointer (c), b.getReadPointer (c) + 512);
    }
    CHECK_NEAR (out[0][100], 0.0, 1.0e-12);                                // Mix 100: no dry signal
    CHECK_NEAR (out[0][100 + 18000] / 0.001, 0.997395, 2.0e-5);              // the kernel's centre (measured 0.997395)
    CHECK_NEAR (out[1][100 + 18000 + 1] / 0.001, 0.031919, 2.0e-5);          // and the tap after it (0.031919)
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

// the factory presets: the reference's 88, each name once, every value inside its parameter's range
static void testPresets()
{
    const auto& list = wdelay::factoryPresets();
    CHECK (list.size() == 88);
    WDelayProcessor p;
    juce::StringArray names;
    bool inRange = true, known = true;
    for (const auto& pr : list)
    {
        names.addIfNotAlreadyThere (pr.name);
        for (const auto& v : pr.values)
        {
            auto* q = p.apvts.getParameter (v.id);
            if (q == nullptr) { known = false; continue; }
            const auto r = q->getNormalisableRange();
            inRange = inRange && v.value >= r.start - 1.0e-3f && v.value <= r.end * 1.0001f;   // (LoPass 20 139 as stored)
        }
    }
    CHECK (names.size() == (int) list.size());
    CHECK (known);
    CHECK (inRange);
    // every preset, applied, reads back its own values (through the tapers)
    int wrong = 0;
    for (int k = 0; k < (int) list.size(); ++k)
    {
        const auto& pr = list[(size_t) k];
        p.setCurrentProgram (k + 1);
        for (const auto& v : pr.values)
        {
            if (juce::String (v.id) == wdelay::mix) continue;
            const float got = p.apvts.getRawParameterValue (v.id)->load();
            if (std::abs (got - v.value) > 1.0e-3f * std::max (1.0f, std::abs (v.value)))
            {
                ++wrong;
                std::printf ("  preset %s: %s is %g, not %g\n", pr.name, v.id, (double) got, (double) v.value);
            }
        }
    }
    CHECK (wrong == 0);
    // a preset's values reach the DSP as stored (the reference's preset load), until the parameter is moved
    auto program = [&list] (const char* name) { for (int k = 0; k < (int) list.size(); ++k) if (juce::String (list[(size_t) k].name) == name) return k + 1; return 0; };
    p.setCurrentProgram (program ("Ping Pong"));
    CHECK (std::abs (p.settings (48000.0).depth - 0.0069f) < 1.0e-6f);
    p.setCurrentProgram (program ("Broken Machine"));
    CHECK (std::abs (p.settings (48000.0).rateHz - 281.7f) < 1.0e-3f);
    CHECK (std::abs (p.settings (48000.0).lopassHz - 20139.0f) < 0.5f);   // stored 20 139: a filter, though shown Off
    p.setCurrentProgram (program ("Full Reset"));
    CHECK (p.settings (48000.0).lopassHz == 0.0f);                         // stored 20 100: Off
    p.setCurrentProgram (program ("Ping Pong"));
    auto* d = p.apvts.getParameter (wdelay::depth);
    d->setValueNotifyingHost (d->convertTo0to1 (0.69f) + 1.0e-4f);           // moved: the parameter at its resolution
    CHECK (std::abs (p.settings (48000.0).depth - 0.01f) < 1.0e-6f);
    {   // the preset's values survive a session's save and reload
        p.setCurrentProgram (program ("Broken Machine"));
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        WDelayProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (q.settings (48000.0).rateHz - 281.7f) < 1.0e-3f);
    }
    CHECK (juce::String (list.front().name) == "Full Reset");
}

// the DSP alone (WDelayDsp.h) against the identified laws
static void testDsp()
{
    using namespace wdelay::dsp;
    const double fs = 48000.0;
    auto run = [fs] (Settings s, std::vector<float> l, std::vector<float> r, int ch = 2)
    {
        Delay d; d.prepare (fs);
        for (size_t i = 0; i < l.size(); i += 256)
        {
            const int n = (int) std::min<size_t> (256, l.size() - i);
            float* w[2] = { l.data() + i, r.data() + i };
            d.process (w, ch, n, s);
        }
        return std::make_pair (l, r);
    };
    auto echo = [] (const std::vector<float>& y, int at) { double e = 0.0; for (int k = -30; k <= 30; ++k) e += y[(size_t) (at + k)]; return e; };
    Settings s; s.timeL = s.timeR = 4800.0; s.feedback = 0.0f;
    // the gain stage: an impulse of a comes back at a / (1 + a)
    for (float a : { 0.001f, 0.5f, 1.0f, 2.0f })
    {
        std::vector<float> l (20000, 0.0f), r (20000, 0.0f); l[100] = r[100] = a;
        auto y = run (s, l, r);
        CHECK_NEAR (echo (y.first, 100 + 4800) / a, 1.0 / (1.0 + a), 1.0e-4);
    }
    // feedback: each echo Feedback / 100 of the one before, one sample later
    {
        Settings f = s; f.feedback = 0.6f;
        std::vector<float> l (30000, 0.0f), r (30000, 0.0f); l[100] = r[100] = 0.0001f;
        auto y = run (f, l, r);
        CHECK_NEAR (echo (y.first, 100 + 2 * 4800 + 1) / echo (y.first, 100 + 4800), 0.6 / (1.0 + 0.0001 * 0.6 * 0.9999), 2.0e-4);
        int pk = 0; for (int k = -5; k <= 5; ++k) if (std::abs (y.first[(size_t) (100 + 2 * 4800 + k)]) > std::abs (y.first[(size_t) (100 + 2 * 4800 + pk)])) pk = k;
        CHECK (pk == 1);
    }
    // Mix: equal power
    {
        Settings m = s; m.mix = 0.25f;
        std::vector<float> l (20000, 0.0f), r (20000, 0.0f); l[100] = r[100] = 0.001f;
        auto y = run (m, l, r);
        CHECK_NEAR (y.first[100] / 0.001, std::cos (0.25 * 1.5707963267948966), 1.0e-5);
        CHECK_NEAR (echo (y.first, 4900) / 0.001, std::sin (0.25 * 1.5707963267948966) * 0.999, 1.0e-4);
    }
    // Ping Pong: the start side, then the other at the same level, then Feedback^2
    {
        Settings pp = s; pp.pingPong = true; pp.feedback = 0.3f;
        std::vector<float> l (40000, 0.0f), r (40000, 0.0f); l[100] = 0.001f;
        auto y = run (pp, l, r);
        CHECK_NEAR (echo (y.first, 4900) / 0.001, 0.7071 * 0.999, 2.0e-3);
        CHECK_NEAR (echo (y.second, 4900), 0.0, 1.0e-9);
        CHECK_NEAR (echo (y.second, 100 + 2 * 4800 + 1) / echo (y.first, 4900), 1.0, 2.0e-3);
        CHECK_NEAR (echo (y.first, 100 + 3 * 4800 + 2) / echo (y.first, 4900), 0.09, 2.0e-3);
    }
    // the glide: from 375 to 100 ms at most twice the speed (the delay falls one sample per sample)
    {
        Settings g = s; g.timeL = g.timeR = 18000.0;
        Delay d; d.prepare (fs);
        std::vector<float> l (512, 0.0f), r (512, 0.0f);
        float* w[2] = { l.data(), r.data() };
        d.process (w, 2, 512, g);                                       // starts at 375 ms
        g.timeL = g.timeR = 4800.0;
        std::vector<float> x (24000, 0.0f), xr (24000, 0.0f);
        x[0] = xr[0] = 0.001f;                                          // written while the delay glides
        float* w2[2] = { x.data(), xr.data() };
        d.process (w2, 2, 24000, g);
        int pk = 0; for (int k = 1; k < 24000; ++k) if (std::abs (x[(size_t) k]) > std::abs (x[(size_t) pk])) pk = k;
        CHECK (pk > 8900 && pk < 9100);                                 // read at d ~ 18000 - t: t = 18000 / 2 = 9000
    }
    // Dual: the right side's own time; without Dual both sides take the left's
    {
        Settings du = s; du.timeR = 7200.0; du.dual = true;
        std::vector<float> l (20000, 0.0f), r (20000, 0.0f); l[100] = r[100] = 0.001f;
        auto y = run (du, l, r);
        CHECK (std::abs (echo (y.second, 7300)) > 0.0009 && std::abs (echo (y.second, 4900)) < 1.0e-6);
        du.dual = false;
        auto z = run (du, l, r);
        CHECK (std::abs (echo (z.second, 4900)) > 0.0009);
    }
    // Mode Mono (one line): the channels do not mix
    {
        Settings mo = s; mo.mode = 1;
        std::vector<float> l (20000, 0.0f), r (20000, 0.0f); l[100] = 0.001f;
        auto y = run (mo, l, r, 1);
        CHECK_NEAR (echo (y.first, 4900) / 0.001, 0.999, 1.0e-4);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testDelayTaper();
    testNotes();
    testTexts();
    testProcessor();
    testDisplayClicks();
    testDsp();
    testPresets();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
