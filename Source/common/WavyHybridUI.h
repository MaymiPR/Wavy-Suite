#pragma once
// Widgets and drawing for Wavy plugins on the Hybrid-line look (WComp): an orange panel, black bakelite knobs (small,
// and large with a chrome skirt), scales printed on the panel, ivory push buttons, a backlit VU meter and an amber
// LED display. Everything is drawn here (no bitmaps); geometry and colours measured on the reference editor
// (reference/wcomp, RESEARCH.md "WComp"). Knobs follow the same mouse feel and selection model as the flat kit.
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "WavyFlatUI.h"

namespace wavy::hybrid
{
// panel and print colours (measured)
inline const juce::Colour kPrint (34, 22, 12), kIvory (226, 223, 208), kIvoryDark (178, 174, 160), kOlive (140, 136, 104),
    kSelText (246, 236, 64), kAmber (255, 150, 30), kNeedle (124, 0, 0), kDialLight (232, 214, 168), kDialDark (186, 158, 104);

juce::Font font (float h, bool bold = true);

// the orange panel with its shading and fine grain (area in design pixels). The rendered panel is kept in `cache`,
// owned by the editor (a function-static image would outlive JUCE and hang the host at unload).
void drawPanel (juce::Graphics&, juce::Rectangle<int> area, juce::Image& cache);

// a scale printed around a knob: ticks from angle a0 to a1 (radians, JUCE convention: clockwise from 12 o'clock)
struct Scale
{
    int ticks = 21;
    float rIn = 36.0f, rOut = 47.0f, thick = 1.8f;
    struct Label { int tick; juce::String text; float radius; float dx = 0.0f, dy = 0.0f; };
    std::vector<Label> labels;
    float labelSize = 11.0f;
};
void drawScale (juce::Graphics&, juce::Point<float> centre, float a0, float a1, const Scale&);

// text printed on the panel, centred on its ink at `centre` (as measured on the reference): Arial Black for the labels,
// Arial Bold for the numbers; em is the font size in pixels, hs its horizontal scale, angle a rotation in radians
void printText (juce::Graphics&, const juce::String&, juce::Point<float> centre, float em, float hs, bool black, float angle = 0.0f);

// A rotary knob in the reference's style. Kind Small: black bakelite, radius 32.4. Large / Huge: a black knob with ten
// flutes in a satin chrome skirt (radius 53.9 / 73). As the reference's filmstrip, the knob turns under a fixed light
// from the upper left: the flutes turn with the value, the shading and the cast shadow stay; rendered from the measured
// shading model (WavyHybridKnobData.h). The pointer is a dark fin with a cream line, shaded by its angle to the light,
// casting a soft shadow that turns with it.
class Knob : public juce::Slider
{
public:
    enum Kind { Small, Large, Huge };
    Knob (Kind k, float startDeg, float endDeg);     // the reference's angles (0 = down, clockwise)
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { if (onHover) onHover (this, true); }
    void mouseExit (const juce::MouseEvent&) override { if (onHover) onHover (this, false); }
    bool hitTest (int x, int y) override;

    const Kind kind;
    juce::Point<float> centre;                        // local
    int steps = 0;                                    // > 0: a stepped knob (Analog)
    wavy::flat::WavesDrag feel { wavy::flat::WavesDrag::Rotary };
    wavy::flat::Selection* selection = nullptr;
    std::function<juce::String (double)> format;
    std::function<void (Knob*, bool)> onHover;
    float ringRadius() const { return kind == Small ? 21.0f : kind == Large ? 30.0f : 41.0f; }
    juce::Rectangle<int> valueArea;                   // the value slot (local): the value box while selected, the entry box
    float radius() const { return kind == Small ? 32.4f : kind == Large ? 53.9f : 73.0f; }
    float pointerAngle() const;
    float paintExtent() const;                        // how far the drawing (shadow, pointer) reaches from the centre

private:
    juce::Image body;                                 // the knob and its cast shadow, rendered for the current scale and turn
    float bodyScale = 0.0f, bodyDelta = 0.0f;
    const juce::Image& bodyImage (float scale, float delta, float& extent);
};

// ivory push buttons in a black frame (a radio group of a choice parameter, or a single toggle). The component's
// bounds are the frame expanded by 4 px (its shadow on the panel).
class ButtonGroup : public juce::Component
{
public:
    ButtonGroup (juce::RangedAudioParameter&, juce::UndoManager*, juce::StringArray labels, bool vertical, bool toggle = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::ParameterAttachment> attachment;
private:
    juce::Rectangle<float> buttonRect (int i) const;
    juce::StringArray labels;
    bool vertical, toggle;
    int selected = 0;
};

// the VU meter's dial and needle, over the housing's black window frame (the housing is part of the panel drawing)
class VuMeter : public juce::Component
{
public:
    VuMeter();
    void paint (juce::Graphics&) override;
    enum class Face { gr, io };                       // the printed scale: gain reduction, or the In / Out level
    void setNeedle (float angleDeg) { if (angleDeg != needle) { needle = angleDeg; repaint(); } }
    void setFace (Face f) { if (f != face) { face = f; repaint(); } }
    juce::Point<float> pivot;                         // local: the scale's centre (the needle turns 2.72, -0.04 px from it)
    float needle = 59.0f;                             // degrees from vertical about the needle's pivot, + to the right (0 dB GR)
    Face face = Face::gr;
private:
    juce::Image dial;                                 // the cream dial's shading, rendered for the current scale
    float dialScale = 0.0f;
};
void drawVuHousing (juce::Graphics&, juce::Rectangle<float> housing, juce::Point<float> archCentre, float archRadius);

// amber LED text in a black window (the BPM display); with digits: a small caption and large 7-segment digits
void drawLedDisplay (juce::Graphics&, juce::Rectangle<float> box, const juce::String& text, const juce::String& digits = {});

// the product logo: a heavy first letter and the rest light ("WCOMP"), from its left edge, centred vertically
void drawLogo (juce::Graphics&, juce::Point<float> leftCentre, const juce::String& first, const juce::String& rest);

// the value entry (double-click) sits on each knob's value slot; the text box is invisible otherwise
class HybridLook : public wavy::flat::FlatLook
{
public:
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};
}
