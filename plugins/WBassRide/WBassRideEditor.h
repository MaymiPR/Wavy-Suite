#pragma once
#include "WBassRideProcessor.h"
#include "WavyRenUI.h"
#include "WavyFlatUI.h"

// WBass Ride editor: Bass Rider's layout on a 320 x 560 px design grid (the narrow Renaissance toolbar included),
// drawn from measurements of the reference (RESEARCH.md, WBass Ride; shading models in WBassRideSkinData.h fitted by
// tools/fit_bassride_parts.py). The static art is rendered once per window scale; the controls draw themselves.
namespace wbassride::ui
{
// the reference's scale on the lower panel: +12 dB at y 222, 0 dB at 327, -12 dB at 432 (8.75 px per dB, measured)
inline float scaleY (float db) { return 327.0f - 8.75f * db; }
inline float dbAtY (float y) { return (327.0f - y) / 8.75f; }

enum class Mark { None, Selected };
void drawMark (juce::Graphics&, juce::Rectangle<float>, Mark, bool round = false);

// the selection as measured on the reference (MeterCapture gl=1 click / hover scripts): a click marks the control in
// yellow until another one is clicked; hovering shows nothing; the selected control's value shows while the mouse is
// on it (or it is dragged)
struct Marks
{
    juce::Component* hover = nullptr;
    juce::Component* selected = nullptr;
    Mark of (const juce::Component* c) const { return c == selected ? Mark::Selected : Mark::None; }
    bool showsValue (const juce::Component* c) const { return c == selected && c == hover; }
    std::function<void()> changed;
};

// the faders' and the Target slider's drag: the thumb follows the mouse on the scale (pxPerUnit), Ctrl a fine mode
// (0.19 of the movement on mouse steps over 2 px, 0.037 on smaller ones, as measured on the references' faders);
// Alt-click resets
struct LinearDrag
{
    bool down (juce::Slider&, const juce::MouseEvent&);
    void drag (juce::Slider&, const juce::MouseEvent&, float pxPerUnit, bool vertical);
    void up (juce::Slider&, const juce::MouseEvent&);
    bool active = false;
    double start = 0.0;
    juce::Point<float> origin, last, virt;
};

// a grey dome knob with a pointer (Sensitivity, Spill, Artifacts): 44 x 44, 270 degrees from Lo to Hi
class Knob : public juce::Slider
{
public:
    Knob();
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { showTextBox(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    wavy::flat::WavesDrag feel { wavy::flat::WavesDrag::Rotary };
    Marks* marks = nullptr;
    double wheelAcc = 0.0;
    bool showing() const { return isMouseButtonDown() || (marks != nullptr && marks->showsValue (this)); }
};

// a vertical fader with a chrome cap (Output, Range Max / Min) or the Rider's ridged cap
class Fader : public juce::Slider
{
public:
    enum Kind { Output, RangeMax, RangeMin, Rider };
    explicit Fader (Kind);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (kind != Rider) showTextBox(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (kind != Rider) wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    juce::Rectangle<float> capBounds() const;                   // local
    const Kind kind;
    LinearDrag feel;
    Marks* marks = nullptr;
    double wheelAcc = 0.0;
    std::function<void (bool)> onGrab;                          // the Rider: grabbed / released
    float yOf (double v) const;                                 // local y of the cap's reference point for a value
};

// the Target slider: a glass window with a red centre line over the Energy meter
class TargetSlider : public juce::Slider
{
public:
    TargetSlider();
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { showTextBox(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    LinearDrag feel;
    Marks* marks = nullptr;
    double wheelAcc = 0.0;
    float energyDb = -100.0f;                                   // the meter under it
    float thumbX (double v) const;                              // local x of the thumb's left edge
};

// the Response switch: Slow / Fast, a chrome block in a dark slot
class Switch : public juce::Button
{
public:
    Switch() : juce::Button ("Response") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool hot, bool down) override;
    void mouseEnter (const juce::MouseEvent& e) override { if (marks != nullptr) { marks->hover = this; if (marks->changed) marks->changed(); } Button::mouseEnter (e); }
    void mouseExit (const juce::MouseEvent& e) override { if (marks != nullptr && marks->hover == this) { marks->hover = nullptr; if (marks->changed) marks->changed(); } Button::mouseExit (e); }
    void mouseDown (const juce::MouseEvent& e) override { if (marks != nullptr) { marks->selected = this; if (marks->changed) marks->changed(); } Button::mouseDown (e); }
    Marks* marks = nullptr;
};
}

class WBassRideEditor : public wavy::ren::Editor
{
public:
    explicit WBassRideEditor (WBassRideProcessor&);
    ~WBassRideEditor() override;

    static constexpr int W = 320, H = 560;

private:
    void paintContent (juce::Graphics&) override;
    void contentMouseDown (const juce::MouseEvent&) override;
    void contentMouseDrag (const juce::MouseEvent&) override;
    void contentMouseUp (const juce::MouseEvent&) override;
    void tick() override;
    void paintStatic (juce::Graphics&) const;                  // the art behind the controls (cached per scale)
    juce::Rectangle<float> idleArrow() const;

    WBassRideProcessor& proc;
    wbassride::ui::Marks marks;
    wbassride::ui::Knob& sensitivity;
    wbassride::ui::Knob& spill;
    wbassride::ui::Knob& artifacts;
    wbassride::ui::Switch& responseSwitch;
    wbassride::ui::TargetSlider& target;
    wbassride::ui::Fader& rangeMax;
    wbassride::ui::Fader& rangeMin;
    wbassride::ui::Fader& output;
    wbassride::ui::Fader& rider;

    juce::Image background;
    float backgroundScale = 0.0f;
    wavy::MeterBallistics energyBar;
    double lastTick = 0.0;
    float energy = -100.0f, ride = 0.0f;
    bool clip = false;
    double clipUntil = 0.0;
    bool draggingIdle = false, idleHover = false;
    float idleGrabOffset = 0.0f;
};
