#pragma once
#include "WVoxChainProcessor.h"
#include "WavyRenUI.h"
#include "WavyFlatUI.h"

// WVox Chain editor: CLA Vocals' layout on a 738 x 474 px design grid (the grey WaveSystem bar included), drawn from
// models fitted on the reference's skin (RESEARCH.md, WVox Chain; WVoxChainSkinData.h, written by
// tools/fit_voxchain_parts.py). The static art (panels, rails and their corners, tracks, screws, grille, printing, WVox
// Chain's name plate, mark and badge) is rendered once per window scale and component; the controls draw themselves
// from their models, each part rendered once per scale. The engineer's signature, the maker's mark and the product
// badge are not drawn.
namespace wvoxchain::ui
{
// a part rendered for one physical scale
struct Sprite
{
    juce::Image image;
    float scale = 0.0f;
};

// the selection as filmed on the reference (MeterCapture gl=1 click / hover / drag scripts, RESEARCH.md): hovering
// shows nothing; a click on a control (a fader's cap, a colour's push button, the Balance knob) marks it (yellow) until
// a click elsewhere: on another control, the background, a meter, a flavour's name or a fader's track off its cap;
// Shift-click on the marked control unmarks it; there is no lasso. The marked control's value shows below it while the
// mouse is on it (and while it is dragged).
struct Marks
{
    juce::Component* selected = nullptr;
    juce::Component* hover = nullptr;
    void select (juce::Component* c) { if (c != selected) { selected = c; if (changed) changed(); } }
    void clear() { select (nullptr); }
    bool showsValue (const juce::Component* c) const { return c != nullptr && c == selected && c == hover; }
    std::function<void()> changed;
    std::function<void (juce::Slider*)> enter;                    // a double-click: type a value
};

inline const juce::Colour activeColour { 228, 215, 106 };          // the marked control's frame (the reference's)

// a channel fader: the cap (34 x 58) on its track, its shadow under it; a click on the cap marks it, Alt-click resets,
// a drag moves the cap with the mouse (Ctrl fine) with the cursor hidden and put back on the cap, a double-click
// types a value; a click on the track off the cap does nothing but unmark
class Fader : public juce::Slider
{
public:
    Fader (bool sensitivityCap, Marks&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { marks.hover = this; if (marks.changed) marks.changed(); }
    void mouseExit (const juce::MouseEvent&) override { if (marks.hover == this) marks.hover = nullptr; if (marks.changed) marks.changed(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    float capTop() const;                                            // the cap's top in local px (the control is 43 x 215)
    bool onCap (juce::Point<float> p) const { return p.x >= 0.0f && p.x < 34.0f && p.y >= capTop() && p.y < capTop() + 58.0f; }
    bool dragging = false;

private:
    const bool sens;
    Marks& marks;
    Sprite cap, shadow;
    double start = 0.0, wheelAcc = 0.0;
    juce::Point<float> origin, last, virt;
};

// a stage's colour: the names of its three flavours in the window (the selected one lit in its colour; the editor
// draws the push button under them, lit while a colour is on); the push button steps through the flavours and marks
// the control; a click on a flavour's name selects it, on the selected one turns the stage's colour off (the
// reference's clear position)
class ColourSelector : public juce::Component
{
public:
    ColourSelector (juce::RangedAudioParameter& param, const Stage& stage, int index, Marks&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setState (int s);
    int state() const;

private:
    juce::RangedAudioParameter& p;
    const Stage& stage;
    const int index;
    Marks& marks;
};

// the input or output meter (stereo bars, or one bar on the Mono/Stereo component's input) with its clip lights; a
// click clears the clip lights (and unmarks the marked control, as a click on the background)
class Meter : public juce::Component
{
public:
    explicit Meter (Marks& m) : marks (m) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clip[0] = clip[1] = false; marks.clear(); repaint(); }
    void set (float l, float r, double dt);
    bool mono = false;
    float shown[2] { -80.0f, -80.0f };
    bool clip[2] { false, false };
    wavy::MeterBallistics ballistics[2];

private:
    Marks& marks;
    Sprite off, on, monoOff, monoOn, clipOff, clipOn, clipMonoOff, clipMonoOn;
};

// the Sensitivity LED over the input meter: off, then green, yellow and red as the level into the chain rises
class SensLed : public juce::Component
{
public:
    explicit SensLed (Marks& m) : marks (m) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { marks.clear(); }
    void set (float levelDb);
    int frame = 0;

private:
    Marks& marks;
    float held = -100.0f;
    std::array<Sprite, 24> frames;
};

// the Balance knob (Stereo component only): a click marks it (a yellow ring round its cap), a vertical drag turns it
// (the reference's knob rate: 1/1000 of the range per px on small mouse steps) with the cursor hidden and put back
// where the drag began, Alt-click resets, a double-click types a value
class BalanceKnob : public juce::Slider
{
public:
    explicit BalanceKnob (Marks&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { marks.hover = this; if (marks.changed) marks.changed(); }
    void mouseExit (const juce::MouseEvent&) override { if (marks.hover == this) marks.hover = nullptr; if (marks.changed) marks.changed(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { feel.drag (*this, e, nullptr); }
    void mouseUp (const juce::MouseEvent& e) override { feel.up (*this, e, e.mouseDownPosition, nullptr); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (marks.enter) marks.enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    bool dragging() const { return feel.active; }

private:
    Marks& marks;
    wavy::flat::WavesDrag feel { wavy::flat::WavesDrag::Rotary };
    double wheelAcc = 0.0;
    std::array<Sprite, 72> frames;
};
}

class WVoxChainEditor : public wavy::ren::Editor
{
public:
    explicit WVoxChainEditor (WVoxChainProcessor&);
    ~WVoxChainEditor() override;
    static constexpr int W = 738, H = 474;

    // the value box under a control (the reference's: a black box in a white frame, its text light grey), shown while
    // the marked control is under the mouse or being dragged
    juce::Rectangle<int> valueBox (const juce::Slider*) const;
    wvoxchain::ui::Marks marks;

protected:
    void paintContent (juce::Graphics&) override;
    void contentMouseDown (const juce::MouseEvent&) override { marks.clear(); }
    void tick() override;

private:
    void showEntry (juce::Slider*);
    void layoutForMode();

    WVoxChainProcessor& proc;
    std::vector<wvoxchain::ui::Fader*> faders;                      // Sensitivity, the six stages, Output
    std::vector<wvoxchain::ui::ColourSelector*> colours;
    wvoxchain::ui::Meter& inMeter;
    wvoxchain::ui::Meter& outMeter;
    wvoxchain::ui::SensLed& led;
    wvoxchain::ui::BalanceKnob& balance;
    std::unique_ptr<juce::TextEditor> entry;
    juce::Slider* entryFor = nullptr;
    juce::Image background;
    float backgroundScale = 0.0f;
    int backgroundMode = -1, laidOut = -1;
    std::array<juce::Image, 6> buttons;                             // the push buttons over the background (per selector)
    std::array<int, 6> buttonLit { { -1, -1, -1, -1, -1, -1 } };
    double lastTick = 0.0;
};
