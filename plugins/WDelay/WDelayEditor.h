#pragma once
#include "WDelayProcessor.h"
#include "WavyRenUI.h"
#include "WavyHybridUI.h"
#include "WavyMeter.h"

// WDelay editor: 749 x 292 px design grid including the toolbar (the reference's Hybrid-line editor), resizable at
// that aspect ratio. Geometry and colours measured on the reference's screenshots (RESEARCH.md "WDelay").
class WDelayEditor : public wavy::ren::Editor
{
public:
    explicit WDelayEditor (WDelayProcessor&);

    static constexpr int W = 749, H = 292;

    // the amber time display: the note value and tempo (Host / BPM), the time in ms, Ping Pong's start side, Dual's
    // two sides. Clicks choose the side the Delay knob sets (Dual) or the Ping Pong start; drags set the value shown.
    class Display : public juce::Component
    {
    public:
        explicit Display (WDelayEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    private:
        juce::RangedAudioParameter* target (juce::Point<float> designPos) const;
        WDelayEditor& ed;
        juce::RangedAudioParameter* dragParam = nullptr;
        float lastY = 0.0f;
        juce::Image glass;                            // the glass, rendered for the current scale
        float glassScale = 0.0f;
    };

    // the rubber tap pad: the time between the last two taps sets the delay (Host: the nearest note; BPM: the tempo)
    class TapPad : public juce::Component
    {
    public:
        explicit TapPad (WDelayEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { pressed = false; repaint(); }
        void deselect() { if (selected) { selected = false; repaint(); } }
    private:
        WDelayEditor& ed;
        bool pressed = false, selected = false;
        double lastTap = 0.0;
        juce::Image up, down;                         // the pad rendered for the current scale
        float imgScale = 0.0f;
    };

    // the knurled Link wheel between the filters: turning it moves HiPass and LoPass by the same amount
    class LinkWheel : public juce::Component
    {
    public:
        explicit LinkWheel (WDelayEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        juce::Point<float> centre;                    // local
    private:
        void move (float normDelta);
        WDelayEditor& ed;
        float lastY = 0.0f, turn = 0.0f;
    };

    // the output meter: a red triangle per channel moving along the printed scale
    class Meter : public juce::Component
    {
    public:
        Meter() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void set (float lDb, float rDb, bool stereo);
    private:
        float l = -100.0f, r = -100.0f;
        bool two = true;
        juce::Image housing;                          // the housing and its scale, rendered for the current scale
        float housingScale = 0.0f;
    };

private:
    void paintContent (juce::Graphics&) override;
    void tick() override;
    float value (const char* id) const;
    void setValue (const char* id, float realValue);
    double tempo() const;                                 // the tempo the note values follow (Host or BPM)
    void updateDelayKnobs();
    void contentMouseDown (const juce::MouseEvent&) override { selectOnly (nullptr); }
    int modeIndex() const { return juce::roundToInt (value (wdelay::mode)); }
    bool mono() const { return modeIndex() == 1; }

    WDelayProcessor& proc;
    wavy::hybrid::Knob& delay;
    wavy::hybrid::Knob& delayNote;
    wavy::hybrid::Knob& delayR;
    wavy::hybrid::Knob& delayNoteR;
    wavy::hybrid::Knob& feedback;
    wavy::hybrid::Knob& depth;
    wavy::hybrid::Knob& rate;
    wavy::hybrid::Knob& hipass;
    wavy::hybrid::Knob& lopass;
    wavy::hybrid::Knob& mix;
    wavy::hybrid::Knob& output;
    wavy::hybrid::Knob& analog;
    Display& display;
    TapPad& tap;
    LinkWheel& link;
    Meter& meter;
    wavy::hybrid::ToggleRow* stereoRow = nullptr;
    wavy::hybrid::ToggleRow* monoRow = nullptr;
    wavy::hybrid::ButtonGroup* syncGroup = nullptr;
    wavy::hybrid::ButtonGroup* lofiButton = nullptr;
    juce::Component* lastPrimary = nullptr;
    void selectOnly (juce::Component*);
    wavy::flat::Selection selection;
    juce::Image panelCache;
    bool rightSide = false;                               // Dual: the Delay knob sets the right side
    int lastSync = -1, lastMode = -1, lastDual = -1, lastPing = -1;
    bool lastRight = false;
    double lastTick = 0.0, clipLevel = 0.0, clipAge = 1.0e9;
    wavy::MeterBallistics meterL, meterR;
    friend class Display;
    friend class TapPad;
    friend class LinkWheel;
};
