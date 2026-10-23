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
// negateTyped: for parameters that are never positive, a typed "18" means -18 (as the references do)
void applyFormat (juce::Slider&, std::function<juce::String (double)> format, bool negateTyped = false);

// The selection, as in the reference (MEASUREMENTS.md "Selection"): a click marks a control (the primary, yellow)
// until another one is clicked; hovering shows nothing. A lasso dragged on the background selects every control it
// touches (live, while dragging); the first in the editor's control order is the primary, the others are members
// (blue). Clicking a member makes it the primary and keeps the group; clicking the background clears it. Dragging any
// member moves the whole group: every member by the same step on its 0..1 parameter scale (each clamped at its own
// ends), choice members (Filter, Monitor) one step per mouse move in the same direction.
struct Selection
{
    std::vector<juce::Component*> members;                    // in the editor's control order
    juce::Component* primary = nullptr;
    // installed by the editor: how a drag of one member moves the others
    std::function<void (juce::Component* source)> gangBegin, gangEnd;
    std::function<void (juce::Component* source, float stepOnReferenceScale)> gangMove;

    void select (juce::Component*);                            // a click
    void setGroup (std::vector<juce::Component*>);             // a lasso (first = primary)
    void clear();
    bool is (const juce::Component* c) const { return c != nullptr && c == primary; }
    bool contains (const juce::Component*) const;
    bool isMember (const juce::Component* c) const { return contains (c) && ! is (c); }
    bool grouped() const { return members.size() > 1; }
private:
    void repaintAll();
};
inline const juce::Colour kSelect (241, 227, 7), kMember (7, 168, 229);
// the selection ring of knobs and the Freq thumb: brightest at 3 and 9 o'clock, gone at the top and bottom
void drawSelectRing (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour = kSelect);
// the lasso: a 1 px white rectangle over everything (does not take the mouse)
class LassoOverlay : public juce::Component
{
public:
    LassoOverlay() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> rect;
};

// How the reference's controls follow the mouse (measured with tools/MeterCapture drag=..., see MEASUREMENTS.md
// "Control feel"): the cursor is hidden while dragging and comes back where the control is; faders and the Freq
// slider move 1:1 with the mouse; knobs take ~500 px for their full range, ~1000 px when the mouse moves slowly
// (<= 3 px per move), and Ctrl gives a fine mode. Alt-click resets to the default; a double-click types a value.
struct WavesDrag
{
    enum Kind { Linear, Rotary };
    explicit WavesDrag (Kind k) : kind (k) {}
    bool down (juce::Slider&, const juce::MouseEvent&, juce::Rectangle<int> textArea, Selection*);  // true: handled
    void drag (juce::Slider&, const juce::MouseEvent&, Selection*);
    bool up (juce::Slider&, const juce::MouseEvent&, juce::Point<float> restoreAt, Selection*);    // true: handled
    const Kind kind;
    bool active = false, ganged = false;
    float referenceScale = 1.0f;     // this control's 0..1 range on the reference's scale (Input: 2/3, it stops at 0 dB)
    juce::Point<float> virt, last;
};

// vertical gain fader: dark track, 17 x 28 thumb with a coloured centre line, value text below
class VFader : public juce::Slider
{
public:
    explicit VFader (juce::Colour line);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    WavesDrag feel { WavesDrag::Linear };
    Selection* selection = nullptr;
    std::function<juce::String (double)> format;
    juce::Colour lineColour;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int travelTop = 27, travelBottom = 192;                // thumb centre range (local y)
    juce::Rectangle<int> textArea { 0, 230, 31, 18 };
    juce::Rectangle<int> editArea { 0, 230, 31, 18 };      // the entry box while typing a value (measured)
};

// rotary knob: grey ring, white arc from the start to the value with an inward tick, dot above; label and value below
class Knob : public juce::Slider
{
public:
    Knob (juce::String label);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    WavesDrag feel { WavesDrag::Rotary };
    Selection* selection = nullptr;
    std::function<juce::String (double)> format;
    juce::String label;
    juce::Point<float> centre { 27.0f, 34.0f };            // local
    void mouseDoubleClick (const juce::MouseEvent&) override;
    float radius = 16.5f;
    juce::Rectangle<int> textArea { 0, 67, 54, 14 };
    juce::Rectangle<int> editArea { 0, 69, 54, 14 };       // the entry box while typing a value (measured)
};

// horizontal slider (Freq): ticks above, white line up to the thumb, ring thumb; value shown while hovered / dragged
class HSlider : public juce::Slider
{
public:
    HSlider();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    WavesDrag feel { WavesDrag::Linear };
    Selection* selection = nullptr;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<juce::String (double)> format;
    float x0 = 7.0f, x1 = 271.0f, y = 10.0f;               // track (local)
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::vector<float> tickPositions;                       // 0..1 along the track
    juce::Rectangle<int> textArea;
    juce::Rectangle<int> editArea { 294, 11, 50, 13 };      // the entry box while typing a value (measured)
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
    Selection* selection = nullptr;
    // the centre line of the box's 1 px border inside the component's bounds (local); the 2 px selection outline runs
    // 2.5 px outside it (measured: border at x 344.5 / 392.5, y 330 / 349; outline pixels 341-342 / 394-395)
    juce::Rectangle<float> box { 4.5f, 5.0f, 48.0f, 19.0f };
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
    Selection* selection = nullptr;
    int rowHeight = 20, buttonHeight = 15;
    int selectLineY = 61;                      // selected: a yellow line under the buttons (local y)
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
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    // the value entry box: white, with a 1 px frame on the left and right only (measured)
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

// drawing helpers
void drawSlopeIcon (juce::Graphics&, juce::Rectangle<float> area, int slope);   // 0: 6 dB/oct, 1: 12, 2: 24
// vertical level meter: rounded dark track, white glowing bar from the bottom; frac 0..1
void drawMeterBar (juce::Graphics&, juce::Rectangle<float> track, float frac);
// the same with the bar's top edge given in pixels (topY >= track bottom: no bar)
void drawMeterBarTo (juce::Graphics&, juce::Rectangle<float> track, float topY);
// filled band curve: points (x, y) left to right; filled down to bottomY with a vertical fade, edge line on top
void drawBand (juce::Graphics&, const std::vector<juce::Point<float>>& pts, float bottomY, juce::Colour c);
}
