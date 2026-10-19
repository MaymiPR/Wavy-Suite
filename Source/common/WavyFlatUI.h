#pragma once
// Widgets for Wavy plugins built on the newer flat WaveSystem look (WBassX, ...): vertical gain faders, rotary
// knobs, a horizontal frequency slider, a slope selector, monitor radio buttons, and drawing helpers for panels,
// level meters and band graphs. Geometry and colours measured from the reference editors (reference/wbassx).
// The widgets draw themselves; FlatLook only supplies the layout and the value text boxes.
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace wavy::flat
{
inline const juce::Colour kPanel (44, 45, 48), kFrame (64, 65, 68), kGap (28, 30, 33), kTrack (22, 22, 24),
    kTick (91, 90, 91), kValue (238, 238, 238), kLabel (205, 205, 207), kRing (90, 91, 91), kDim (150, 150, 152),
    kWhite (246, 246, 246), kBlue (93, 160, 230), kOrange (235, 142, 104), kBadgeBlue (62, 140, 215),
    kBadgeOrange (213, 140, 106);

juce::Font font (float h, bool bold = false);

// the reference's gain fader taper (normalised 0..1 -> dB): 40 log10(1.5 v) up to 0 dB at v = 2/3, then 8u + 4u^2
// with u = 3v - 2 up to +12 dB at v = 1; -inf at 0
float gainDbFromNorm (float v);
float gainNormFromDb (float db);
// a dB parameter range with that taper (-100 dB stands for -inf), up to maxDb (0 or +12)
juce::NormalisableRange<float> gainRange (float maxDb);
juce::String gainText (float db);                          // "-inf" below -99.9 dB
// installs a widget's own value text (the parameter attachment puts the parameter's, with units) and a parser that
// reads plain numbers and "-inf"
void applyFormat (juce::Slider&, std::function<juce::String (double)> format);

// vertical gain fader: dark track, 17 x 28 thumb with a coloured centre line, value text below
class VFader : public juce::Slider
{
public:
    explicit VFader (juce::Colour line);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<juce::String (double)> format;
    juce::Colour lineColour;
    int travelTop = 27, travelBottom = 192;                // thumb centre range (local y)
    juce::Rectangle<int> textArea { 0, 230, 31, 18 };
};

// rotary knob: grey ring, white arc from the start to the value with an inward tick, dot above; label and value below
class Knob : public juce::Slider
{
public:
    Knob (juce::String label);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<juce::String (double)> format;
    juce::String label;
    juce::Point<float> centre { 27.0f, 34.0f };            // local
    float radius = 16.5f;
    juce::Rectangle<int> textArea { 0, 66, 54, 14 };
};

// horizontal slider (Freq): ticks above, white line up to the thumb, ring thumb; value shown while hovered / dragged
class HSlider : public juce::Slider
{
public:
    HSlider();
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<juce::String (double)> format;
    float x0 = 7.0f, x1 = 271.0f, y = 10.0f;               // track (local)
    std::vector<float> tickPositions;                       // 0..1 along the track
    juce::Rectangle<int> textArea;
};

// slope selector (Filter): rounded box with the slope icon and a menu arrow. As in the reference, a click on the
// slope picture steps to the next slope (6 -> 12 -> 24 -> 6 dB/oct); a click on the arrow opens the list.
class SlopeBox : public juce::ComboBox
{
public:
    SlopeBox() { setWantsKeyboardFocus (false); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    static constexpr int arrowWidth = 20;
private:
    bool stepping = false;
};

// radio buttons for a choice parameter (Monitor), one per row
class RadioColumn : public juce::Component
{
public:
    RadioColumn (juce::RangedAudioParameter&, juce::UndoManager*, juce::StringArray labels, std::vector<juce::Colour> accents);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int rowHeight = 20, buttonHeight = 15;
private:
    juce::StringArray labels;
    std::vector<juce::Colour> accents;
    int selected = 0;
};

class FlatLook : public juce::LookAndFeel_V4
{
public:
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override { return font (13.0f); }
};

// drawing helpers
void drawSlopeIcon (juce::Graphics&, juce::Rectangle<float> area, int slope);   // 0: 6 dB/oct, 1: 12, 2: 24
// vertical level meter: rounded dark track, white glowing bar from the bottom; frac 0..1
void drawMeterBar (juce::Graphics&, juce::Rectangle<float> track, float frac);
// filled band curve: points (x, y) left to right; filled down to bottomY with a vertical fade, edge line on top
void drawBand (juce::Graphics&, const std::vector<juce::Point<float>>& pts, float bottomY, juce::Colour c);
}
