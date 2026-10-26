#pragma once
#include "WBassRiderProcessor.h"
#include "WavyRenUI.h"
#include "WavyFlatUI.h"

// WBass Rider editor: Bass Rider's layout on a 320 x 560 px design grid (the narrow Renaissance toolbar included),
// drawn from measurements of the reference (RESEARCH.md, WBass Rider; shading models in WBassRiderSkinData.h fitted by
// tools/fit_bassride_parts.py). The static art is rendered once per window scale; the controls draw themselves.
namespace wbassrider::ui
{
// the reference's scale on the lower panel: +12 dB at y 222, 0 dB at 327, -12 dB at 432 (8.75 px per dB, measured)
inline float scaleY (float db) { return 327.0f - 8.75f * db; }
inline float dbAtY (float y) { return (327.0f - y) / 8.75f; }

enum class Mark { None, Selected, Member };                    // yellow, blue
void drawMark (juce::Graphics&, juce::Rectangle<float>, Mark, bool round = false);

// the selection as measured on the reference (MeterCapture gl=1 click / hover / lasso scripts): hovering shows nothing;
// a click marks the control in yellow (the Rider and the Idle arrow in blue) until another one is clicked. A lasso
// dragged on the background selects every control it touches: the first in the reference's control order in yellow,
// the others in blue; a click on a member makes it the yellow one, and a drag on a member moves nothing (as the
// reference). The selected control's value shows while the mouse is on it (in a group: every member's value).
struct Marks
{
    juce::Component* hover = nullptr;
    juce::Component* selected = nullptr;
    std::vector<juce::Component*> group;                        // a lasso's controls (2 or more)
    std::vector<const juce::Component*> blueAlone;              // drawn blue when selected alone (Rider, Idle arrow)
    bool inGroup (const juce::Component* c) const { return std::find (group.begin(), group.end(), c) != group.end(); }
    Mark of (const juce::Component* c) const
    {
        if (c == nullptr) return Mark::None;
        if (group.size() > 1) return c == selected ? Mark::Selected : inGroup (c) ? Mark::Member : Mark::None;
        if (c != selected) return Mark::None;
        return std::find (blueAlone.begin(), blueAlone.end(), c) != blueAlone.end() ? Mark::Member : Mark::Selected;
    }
    bool showsValue (const juce::Component* c) const
    {
        return hover != nullptr && hover == selected && (c == selected || (group.size() > 1 && inGroup (c)));
    }
    // a click on a control: false when it is a member of a group (its drag then moves nothing)
    bool click (juce::Component* c)
    {
        const bool member = group.size() > 1 && inGroup (c);
        if (! member) group.clear();
        selected = c;
        if (changed) changed();
        return ! member;
    }
    std::function<void()> changed;
    std::function<void (juce::Slider*)> enter;                  // a double-click: type a value (the editor's box)
};

// the faders' and the Target slider's drag: the thumb follows the mouse on the scale (pxPerUnit), Ctrl a fine mode
// (0.19 of the movement on mouse steps over 2 px, 0.037 on smaller ones, as measured on the references' faders);
// Alt-click resets; the cursor is hidden while dragging and comes back where the thumb went (measured)
struct LinearDrag
{
    bool down (juce::Slider&, const juce::MouseEvent&);
    void drag (juce::Slider&, const juce::MouseEvent&, float pxPerUnit, bool vertical);
    void up (juce::Slider&, const juce::MouseEvent&);
    bool active = false, vertical = true;
    double start = 0.0;
    float pxPerUnit = 1.0f;
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
    void mouseDoubleClick (const juce::MouseEvent&) override { if (marks != nullptr && marks->enter) marks->enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    wavy::flat::WavesDrag feel { wavy::flat::WavesDrag::Rotary };
    bool blocked = false;                                       // a group member's drag (moves nothing)
    Marks* marks = nullptr;
    double wheelAcc = 0.0;
    bool showing() const { return isMouseButtonDown() || (marks != nullptr && marks->showsValue (this)); }
private:
    const juce::Image& dome (float scale);
    juce::Image domeImage;
    float domeScale = 0.0f;
public:
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
    void mouseDoubleClick (const juce::MouseEvent&) override { if (kind != Rider && marks != nullptr && marks->enter) marks->enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (kind != Rider) wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    juce::Rectangle<float> capBounds() const;                   // local
    const Kind kind;
    LinearDrag feel;
    bool blocked = false;                                       // a group member's drag (moves nothing)
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
    void mouseDoubleClick (const juce::MouseEvent&) override { if (marks != nullptr && marks->enter) marks->enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    LinearDrag feel;
    bool blocked = false;                                       // a group member's drag (moves nothing)
    Marks* marks = nullptr;
    double wheelAcc = 0.0;
    float energyDb = -100.0f;                                   // the meter under it
    float thumbX (double v) const;                              // local x of the thumb's left edge
};

// the Idle arrow's column (the reference's Idle and IdleHover controls, 35 .. 47 x 222 .. 433): the arrow at the Idle
// value, dragged up and down within the range; while the mouse is on the column its value shows at the left edge
class IdleColumn : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<float()> idleDb;                              // the arrow's value
    std::function<void (float db)> setIdleDb;                   // a drag (inside a gesture)
    std::function<void (bool begin)> gesture;
    std::function<void()> reset;                                // Alt-click
    bool blocked = false;                                       // a group member's drag (moves nothing)
    Marks* marks = nullptr;
    float grab = 0.0f;
    bool dragging = false;
};

// the Response switch: Slow / Fast, a chrome block in a dark slot
class Switch : public juce::Button
{
public:
    Switch() : juce::Button ("Response") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool hot, bool down) override;
    void mouseEnter (const juce::MouseEvent& e) override { if (marks != nullptr) { marks->hover = this; if (marks->changed) marks->changed(); } Button::mouseEnter (e); }
    void mouseExit (const juce::MouseEvent& e) override { if (marks != nullptr && marks->hover == this) { marks->hover = nullptr; if (marks->changed) marks->changed(); } Button::mouseExit (e); }
    void mouseDown (const juce::MouseEvent& e) override { blocked = marks != nullptr && ! marks->click (this); if (! blocked) Button::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! blocked) Button::mouseUp (e); blocked = false; }
    bool blocked = false;
    Marks* marks = nullptr;
};
}

class WBassRiderEditor : public wavy::ren::Editor
{
public:
    explicit WBassRiderEditor (WBassRiderProcessor&);
    ~WBassRiderEditor() override;

    static constexpr int W = 320, H = 560;

private:
    void paintContent (juce::Graphics&) override;
    void tick() override;
    void contentMouseDown (const juce::MouseEvent&) override;
    void contentMouseDrag (const juce::MouseEvent&) override;
    void contentMouseUp (const juce::MouseEvent&) override;
    void paintStatic (juce::Graphics&) const;                  // the art behind the controls (cached per scale)
    // typing a value (measured on the reference: a double-click on a knob, Target, Output or a Range thumb opens the
    // entry in the control's value slot; Enter applies a value inside the range, others are ignored; Spill reads a
    // typed positive number as negative; the Rider and the Idle arrow take none)
    void openEntry (juce::Slider*);
    juce::Rectangle<int> entryArea (juce::Slider*) const;
    void closeEntry (bool apply);
    std::unique_ptr<juce::TextEditor> entry;
    juce::Slider* entryFor = nullptr;
    // the lasso
    wavy::flat::LassoOverlay& lasso;
    juce::Point<float> lassoStart;
    bool lassoing = false;

    WBassRiderProcessor& proc;
    wbassrider::ui::Marks marks;
    wbassrider::ui::Knob& sensitivity;
    wbassrider::ui::Knob& spill;
    wbassrider::ui::Knob& artifacts;
    wbassrider::ui::Switch& responseSwitch;
    wbassrider::ui::TargetSlider& target;
    wbassrider::ui::Fader& rangeMax;
    wbassrider::ui::Fader& rangeMin;
    wbassrider::ui::Fader& output;
    wbassrider::ui::Fader& rider;
    wbassrider::ui::IdleColumn& idleColumn;

    juce::Image background;
    float backgroundScale = 0.0f;
    double lastTick = 0.0;
    float energy = -100.0f, ride = 0.0f;
    bool clip = false;                                          // latched until clicked
};
