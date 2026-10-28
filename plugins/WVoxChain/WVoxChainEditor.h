#pragma once
#include "WVoxChainProcessor.h"
#include "WavyRenUI.h"

// WVox Chain editor: CLA Vocals' layout on a 738 x 474 px design grid (the grey WaveSystem bar included), drawn from
// models fitted on the reference's skin (RESEARCH.md, WVox Chain; WVoxChainSkinData.h, written by
// tools/fit_voxchain_parts.py). The static art (panels, tracks, screws, grille, printing, WVox Chain's name plate) is
// rendered once per window scale and component; the controls draw themselves from their models, each part rendered
// once per scale. The engineer's signature, the maker's mark and the product badge are not drawn.
namespace wvoxchain::ui
{
// a part rendered for one physical scale
struct Sprite
{
    juce::Image image;
    float scale = 0.0f;
};

// a channel fader: the cap (34 x 58) on its track, its shadow under it; the cap follows the mouse, Ctrl fine, Alt-click
// resets, a double-click types a value; the value shows below the fader while the mouse is over it
class Fader : public juce::Slider
{
public:
    explicit Fader (bool sensitivityCap);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); if (hoverChanged) hoverChanged(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); if (hoverChanged) hoverChanged(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (enter) enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    float capTop() const;                                            // the cap's top in local px (the control is 43 x 215)
    bool hover = false, dragging = false;
    std::function<void()> hoverChanged;
    std::function<void (juce::Slider*)> enter;

private:
    const bool sens;
    Sprite cap, shadow;
    double start = 0.0, wheelAcc = 0.0;
    juce::Point<float> origin, last, virt;
};

// a stage's colour: the window of its three flavours (the selected one lit in its colour) and the push button that
// steps through them (lit while a colour is on); a click on a flavour's name selects it, on the selected one turns
// the stage's colour off (the reference's clear position)
class ColourSelector : public juce::Component
{
public:
    ColourSelector (juce::RangedAudioParameter& param, const Stage& stage);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setState (int s);
    int state() const;

private:
    juce::RangedAudioParameter& p;
    const Stage& stage;
    Sprite window, buttonOff, buttonOn;
};

// the input or output meter (stereo bars, or one bar on the Mono/Stereo component's input) with its clip lights
class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clip[0] = clip[1] = false; repaint(); }
    void set (float l, float r, double dt);
    bool mono = false;
    float shown[2] { -80.0f, -80.0f };
    bool clip[2] { false, false };
    wavy::MeterBallistics ballistics[2];

private:
    Sprite off, on, monoOff, monoOn, clipOff, clipOn, clipMonoOff, clipMonoOn;
};

// the Sensitivity LED over the input meter: off, then green, yellow and red as the level into the chain rises
class SensLed : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void set (float levelDb);
    int frame = 0;

private:
    float held = -100.0f;
    std::array<Sprite, 24> frames;
};

// the Balance knob (Stereo component only)
class BalanceKnob : public juce::Slider
{
public:
    BalanceKnob();
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; if (hoverChanged) hoverChanged(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; if (hoverChanged) hoverChanged(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (enter) enter (this); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { wavy::wavesWheel (*this, e, w, wheelAcc, false); }
    bool hover = false;
    std::function<void()> hoverChanged;
    std::function<void (juce::Slider*)> enter;

private:
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

    // the value box under a control (the reference's: a black box, its text light grey), shown while the mouse is on it
    juce::Rectangle<int> valueBox (const juce::Slider*) const;

protected:
    void paintContent (juce::Graphics&) override;
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
    double lastTick = 0.0;
};
