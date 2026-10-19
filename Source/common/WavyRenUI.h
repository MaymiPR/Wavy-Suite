#pragma once
// Shared UI for the Wavy plugins built on the reference's "Renaissance" dark skin (WVox, WBass, ...):
// colours, fonts, faders (big capsule, small round, horizontal), the IN switch, and an editor base class that does
// the design-grid scaling, the toolbar, resizing and the common drawing (panel, housing, meter window, capsules).
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "WavyToolbar.h"

namespace wavy::ren
{
// colours measured from the reference's dark skin as the real plugins render it (reference/shots)
inline const juce::Colour kFrameLine (52, 71, 86), kPanel (5, 29, 48), kOuterFill (4, 34, 55), kOuterLine (29, 55, 74),
    kInnerLine (51, 84, 108), kInnerFill (7, 20, 31), kWindow (9, 47, 78), kGrid (44, 76, 103),
    kLabel (129, 166, 195), kScale (56, 89, 113), kScaleInner (73, 105, 129), kMeterBg (10, 61, 102), kMeterFg (248, 150, 0),
    kGrRed (120, 34, 35), kGrRedLine (139, 65, 66), kValue (211, 213, 215), kReadout (130, 166, 195),
    kRing (244, 134, 2), kRingActive (255, 255, 0), kTrack (73, 102, 124);

juce::Font arial (float h, bool bold = false);
juce::String dbText (float v, bool infAtMinus80 = false);

// Big: the centre capsule fader (102 px wide). Small: the round fader on a 33 px column.
// Horizontal: the round knob on a horizontal track.
class Fader : public juce::Slider
{
public:
    enum Kind { Big, Small, Horizontal };
    Fader (Kind k, int thumbOffsetX = 4);

    // text shown / typed; negateTyped: a typed positive number is read as negative (dB controls)
    std::function<juce::String (double)> format = [] (double v) { return dbText ((float) v); };
    bool negateTyped = true;
    void applyFormat();

    void valueChanged() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    juce::Rectangle<int> valueTextBounds() const;

    const Kind kind;
    const int offsetX;
    juce::Rectangle<int> textArea { 0, 0, 42, 16 };   // Small / Horizontal: where the value text sits (local)
    int trackTop = 16;                                 // Small: local y of the 33 x 195 control rectangle
};

class Look : public juce::LookAndFeel_V4
{
public:
    int getSliderThumbRadius (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

// the reference's "IN" pill switch (33 x 38 control, pill at the top)
class InSwitch : public juce::Button
{
public:
    InSwitch() : juce::Button ("IN") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool hot, bool down) override;
};

class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    Editor (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&, juce::UndoManager&,
            juce::RangedAudioParameter* modeParam, const juce::String& product, const juce::String& version,
            int designW, int designH, wavy::Toolbar::Style toolbarStyle = wavy::Toolbar::Style::Renaissance);
    ~Editor() override;
    void resized() override;
    void parentHierarchyChanged() override;

protected:
    // subclasses draw and react in design coordinates
    virtual void paintContent (juce::Graphics&) = 0;
    virtual void contentMouseDown (const juce::MouseEvent&) {}
    virtual void tick() {}                                         // 60 Hz: read meters, repaint

    // controls are owned here, so they outlive their parameter attachments during teardown
    Fader& makeFader (Fader::Kind, int thumbOffsetX = 4);
    InSwitch& makeSwitch();
    void attach (Fader&, const char* paramID, juce::Rectangle<int> bounds, double resetValue);
    void attach (juce::Button&, const char* paramID, juce::Rectangle<int> bounds);
    juce::Component& content();
    bool isMono() const;
    void setFactoryPresets (const std::vector<wavy::FactoryPreset>& list) { toolbar->setFactoryPresets (&list); }

    // for editors with their own widgets (e.g. the flat kit, WavyFlatUI.h): the editor owns them and their look,
    // and keeps the parameter attachments; everything is torn down before the look
    template <class T, class... Args> T& make (Args&&... args)
    {
        owned.push_back (std::make_unique<T> (std::forward<Args> (args)...));
        return static_cast<T&> (*owned.back());
    }
    void setAuxLook (std::unique_ptr<juce::LookAndFeel> l) { auxLook = std::move (l); }
    juce::LookAndFeel* getAuxLook() const { return auxLook.get(); }
    void attachSlider (juce::Slider&, const char* paramID, juce::Rectangle<int> bounds, double resetValue);
    void attachCombo (juce::ComboBox&, const char* paramID, juce::Rectangle<int> bounds);
    void addToContent (juce::Component&, juce::Rectangle<int> bounds);
    void keepAttachment (std::unique_ptr<juce::ParameterAttachment> a) { paramAtts.push_back (std::move (a)); }

    // common drawing
    void paintPanel (juce::Graphics&) const;                      // black background, panel, frame
    static void paintHousing (juce::Graphics&);                    // big rounded housing + inner frame
    static void paintWindow (juce::Graphics&, float fillFromTop, juce::Colour fill, juce::Colour fillLine,
                             int lines, float firstLineY, float lineStep, bool fromBottom = false);
    static void paintCapsule (juce::Graphics&, juce::Rectangle<float>, float frac);
    static void paintTitle (juce::Graphics&, const juce::String& name, juce::Colour nameColour);   // script W + name, centred
    static void paintClipLight (juce::Graphics&, juce::Rectangle<float>, bool lit);

    juce::AudioProcessorValueTreeState& apvts;
    juce::UndoManager& undo;
    const int W, H;

private:
    class Content;
    void timerCallback() override { toolbar->refresh(); tick(); }

    Look look;
    std::unique_ptr<juce::LookAndFeel> auxLook;           // destroyed after the widgets that use it (declared first)
    std::unique_ptr<Content> contentComp;
    std::unique_ptr<wavy::Toolbar> toolbar;
    juce::RangedAudioParameter* modeParam;
    juce::ComponentBoundsConstrainer constrainer;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAtts;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAtts;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> paramAtts;
    std::vector<Fader*> faders;
    std::vector<std::unique_ptr<juce::Component>> owned;
};
}
