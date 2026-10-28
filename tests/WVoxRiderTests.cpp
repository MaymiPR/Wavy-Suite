// Unit tests for WVox Rider (Phase 2): the parameters as the reference's, the buses (a side-chain), no latency, the
// audio path, Automation (Read plays the host's Rider Fader, Write sends the ride), the editor (Studio / Live layouts,
// the Music knob's disabled look without a side-chain).
// Run: WVoxRiderTests (exit code 0 = all passed).
#include <functional>
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WVoxRiderProcessor.h"
#include "WVoxRiderEditor.h"

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

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testParameters();
    testBuses();
    testPathAndRead();
    testWrite();
    testEditor();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
