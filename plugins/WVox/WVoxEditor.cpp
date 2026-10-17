#include "WVoxEditor.h"
#include "WavyLogo.h"
#include "WavySettings.h"

using namespace juce;

namespace
{
// Colours measured from the reference's dark skin.
const Colour kFrameLine (52, 71, 86), kPanel (5, 29, 48), kOuterFill (4, 34, 55), kOuterLine (29, 55, 74),
             kInnerLine (51, 84, 108), kInnerFill (7, 20, 31), kWindow (9, 47, 78), kGrid (44, 76, 103),
             kLabel (111, 146, 173), kScale (60, 90, 114), kMeterBg (10, 61, 102), kMeterFg (248, 150, 0),
             kGrSlate (33, 54, 71), kValue (211, 213, 215), kReadout (90, 90, 90);

Font arial (float h, bool bold = false) { return Font (FontOptions ("Arial", h, bold ? Font::bold : Font::plain)); }

String dbText (float v, bool inf80 = false)
{
    if (inf80 && v <= -79.95f) return "-inf";
    return String (v, 1);
}
}

//==============================================================================
class WVoxEditor::Fader : public Slider
{
public:
    enum Kind { Comp, Gate, Gain };
    explicit Fader (Kind k) : kind (k)
    {
        setSliderStyle (LinearVertical);
        setTextBoxStyle (TextBoxBelow, false, 40, 16);
        setTextBoxIsEditable (true);
        for (auto* c : getChildren())
            if (auto* l = dynamic_cast<Label*> (c)) l->setEditable (false, true);   // double-click to type
        setSliderSnapsToMousePosition (false);
        setNumDecimalPlacesToDisplay (1);
        setScrollWheelEnabled (true);
        setMouseDragSensitivity (150);
        textFromValueFunction = [k] (double v) { return k == Gate ? dbText ((float) v, true) : dbText ((float) v); };
        valueFromTextFunction = [] (const String& s) { return (double) -std::abs (s.trim().getFloatValue()); };
    }
    void valueChanged() override { Slider::valueChanged(); if (kind == Comp) resized(); }
    const Kind kind;
};

class WVoxEditor::Look : public LookAndFeel_V4
{
public:
    int getSliderThumbRadius (Slider& s) override { return static_cast<Fader&> (s).kind == Fader::Comp ? 32 : 20; }

    Slider::SliderLayout getSliderLayout (Slider& s) override
    {
        auto& f = static_cast<Fader&> (s);
        Slider::SliderLayout l;
        if (f.kind == Fader::Comp)
        {
            l.sliderBounds = { 0, 32, 102, 143 };                    // range of the thumb centre
            const float pos = s.getPositionOfValue (s.getValue());   // centre of the thumb
            l.textBoxBounds = { 26, (int) std::round (pos - 32.0f) + 17, 50, 21 };
        }
        else
        {
            l.sliderBounds = { f.kind == Fader::Gate ? 4 : 5, 36, 33, 155 };   // range of the thumb centre
            l.textBoxBounds = { 0, 0, 42, 16 };
        }
        return l;
    }

    Label* createSliderTextBox (Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        const bool comp = static_cast<Fader&> (s).kind == Fader::Comp;
        l->setFont (arial (comp ? 13.5f : 13.5f));
        l->setColour (Label::textColourId, comp ? kValue : kReadout);
        l->setColour (Label::textWhenEditingColourId, Colours::white);
        l->setColour (Label::backgroundColourId, Colours::transparentBlack);
        l->setColour (Label::outlineColourId, Colours::transparentBlack);
        l->setColour (TextEditor::backgroundColourId, Colour (0, 0, 20));
        l->setColour (TextEditor::textColourId, Colours::white);
        l->setBorderSize ({ 0, 0, 0, 0 });
        l->setJustificationType (Justification::centred);
        l->setInterceptsMouseClicks (false, false);
        return l;
    }

    void drawLinearSlider (Graphics& g, int, int, int, int, float pos, float, float, Slider::SliderStyle, Slider& s) override
    {
        auto& f = static_cast<Fader&> (s);
        const bool down = s.isMouseButtonDown();
        const Colour ring = down ? Colour (255, 255, 0) : Colour (244, 134, 2);
        if (f.kind == Fader::Comp)
        {
            const float top = pos - 32.0f, cy = top + 27.0f;
            const float left = 0.0f;
            // rails (full width) and capsule: orange ring 3 px, slate body
            g.setColour (Colour (244, 134, 2));
            Rectangle<float> pill (left + 22.0f, top + 11.0f, 58.0f, 33.0f);
            g.fillRect (left, cy - 1.5f, pill.getX() + 4.0f, 3.0f);                      // rails stop at the ring
            g.fillRect (pill.getRight() - 4.0f, cy - 1.5f, 102.0f - pill.getRight() + 4.0f, 3.0f);
            // transparent inside: only the ring (and the rails) are drawn
            g.setColour (Colour (244, 134, 2));
            g.drawRoundedRectangle (pill.reduced (1.5f), 15.0f, 3.0f);
            auto in = pill.reduced (3.0f);
            if (down) { g.setColour (ring); g.drawRoundedRectangle (in.reduced (0.5f), in.getHeight() * 0.5f, 1.5f); }
        }
        else
        {
            const float cx = 16.5f + (float) (f.kind == Fader::Gate ? 4 : 5), cy = pos - 3.0f;
            g.setColour (ring);                                       // ring only, transparent inside
            g.drawEllipse (cx - 12.5f, cy - 12.5f, 25.0f, 25.0f, 2.0f);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.fillRect (cx - 8.5f, cy - 0.5f, 17.0f, 1.0f);
        }
    }
};

// Holds everything at the 371 x 357 design size; the editor scales it with a transform.
class WVoxEditor::Content : public Component
{
public:
    explicit Content (WVoxEditor& e) : ed (e) {}
    void paint (Graphics& g) override { ed.paintContent (g); }
    void mouseDown (const MouseEvent& e) override { ed.contentMouseDown (e); }
    WVoxEditor& ed;
};

//==============================================================================
WVoxEditor::WVoxEditor (WVoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), look (std::make_unique<Look>()), content (std::make_unique<Content> (*this)),
      toolbar (std::make_unique<wavy::Toolbar> (p.undo, p.apvts, StringArray { "Stereo", "Mono" }, p.apvts.getParameter (wvox::mode),
                                                "Wavy WVox", JucePlugin_VersionString))
{
    addAndMakeVisible (*content);
    content->setBounds (0, 0, W, H);
    comp = std::make_unique<Fader> (Fader::Comp);
    gate = std::make_unique<Fader> (Fader::Gate);
    gain = std::make_unique<Fader> (Fader::Gain);
    for (auto* f : { comp.get(), gate.get(), gain.get() })
    {
        f->setLookAndFeel (look.get());
        content->addAndMakeVisible (*f);
    }
    comp->setBounds (135, 99, 102, 207);
    gate->setBounds (37, 89, 42, 211);
    gain->setBounds (292, 89, 42, 211);

    auto& a = p.apvts;
    compAtt = std::make_unique<SliderParameterAttachment> (*a.getParameter (wvox::comp), *comp, &p.undo);
    gateAtt = std::make_unique<SliderParameterAttachment> (*a.getParameter (wvox::gate), *gate, &p.undo);
    gainAtt = std::make_unique<SliderParameterAttachment> (*a.getParameter (wvox::gain), *gain, &p.undo);
    for (auto* f : { comp.get(), gate.get(), gain.get() })   // the attachment installs its own "-80.0 dB" text
    {
        const auto k = f->kind;
        f->textFromValueFunction = [k] (double v) { return k == Fader::Gate ? dbText ((float) v, true) : dbText ((float) v); };
        f->valueFromTextFunction = [] (const String& t) { return (double) -std::abs (t.trim().getFloatValue()); };
        f->updateText();
    }
    comp->setDoubleClickReturnValue (true, 0.0);
    gate->setDoubleClickReturnValue (true, -80.0);
    gain->setDoubleClickReturnValue (true, 0.0);

    content->addAndMakeVisible (*toolbar);
    toolbar->setTopLeftPosition (0, 0);
    toolbar->getWindowScale = [this] { return (float) getWidth() / (float) W; };
    toolbar->setWindowScale = [this] (float s) { setSize (juce::roundToInt (W * s), juce::roundToInt (H * s)); };

    // resizable, fixed aspect ratio; the size is remembered with the plugin state
    constrainer.setMinimumSize (W * 3 / 4, H * 3 / 4);
    constrainer.setMaximumSize (W * 3, H * 3);
    constrainer.setFixedAspectRatio ((double) W / H);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const int savedW = (int) p.apvts.state.getProperty ("uiWidth", juce::roundToInt (W * wavy::defaultWindowScale()));
    setSize (jlimit (W * 3 / 4, W * 3, savedW), jlimit (H * 3 / 4, H * 3, savedW * H / W));
    startTimerHz (30);
}

void WVoxEditor::resized()
{
    content->setTransform (AffineTransform::scale ((float) getWidth() / (float) W));
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}

WVoxEditor::~WVoxEditor()
{
    for (auto* f : { comp.get(), gate.get(), gain.get() }) f->setLookAndFeel (nullptr);
}

bool WVoxEditor::isMono() const { return proc.apvts.getRawParameterValue (wvox::mode)->load() > 0.5f; }

Rectangle<int> WVoxEditor::vuRect (bool right) const { return { isMono() ? 311 : (right ? 315 : 307), 122, 5, 155 }; }
Rectangle<int> WVoxEditor::vuTextRect (bool right) const { return { isMono() ? 296 : (right ? 315 : 278), 303, 33, 14 }; }

void WVoxEditor::timerCallback()
{
    energy = proc.energyDb.load(); attn = proc.attnDb.load();
    outL = proc.outLDb.load();     outR = proc.outRDb.load();
    energyHold = jmax (energyHold, energy); attnHold = jmin (attnHold, attn);
    outLHold = jmax (outLHold, outL);       outRHold = jmax (outRHold, outR);
    toolbar->refresh();
    content->repaint (30, 100, 310, 240);
}

void WVoxEditor::contentMouseDown (const MouseEvent& e)
{
    const auto p = e.getPosition();   // click a readout to reset its peak hold
    if (Rectangle<int> (39, 303, 33, 14).contains (p)) energyHold = -80.0f;
    if (Rectangle<int> (148, 287, 74, 23).contains (p)) attnHold = 0.0f;
    if (vuTextRect (false).contains (p)) outLHold = -80.0f;
    if (! isMono() && vuTextRect (true).contains (p)) outRHold = -80.0f;
}

void WVoxEditor::paintContent (Graphics& g)
{
    g.fillAll (Colours::black);

    // panel
    const Rectangle<float> panel (1.0f, 36.0f, 369.0f, 320.0f);
    g.setColour (kPanel);
    g.fillRect (panel);
    {
        ColourGradient glow (Colour (0, 9, 18).withAlpha (0.55f), 185.0f, 200.0f, Colour (0, 9, 18).withAlpha (0.0f), 185.0f, 360.0f, true);
        glow.addColour (0.55, Colour (0, 9, 18).withAlpha (0.28f));
        g.setGradientFill (glow);
        g.fillRect (panel);
    }
    g.setColour (kFrameLine);
    g.drawRect (panel, 1.0f);

    // logo: script W + VOX
    g.setColour (Colour (170, 176, 182));
    g.setFont (Font (FontOptions ("Georgia", 26.0f, Font::italic)));
    g.drawText ("W", Rectangle<int> (146, 46, 34, 30), Justification::centred);
    g.setColour (Colour (166, 28, 45));
    g.setFont (arial (22.0f));
    g.drawText ("VOX", Rectangle<int> (178, 49, 48, 26), Justification::centredLeft);

    // big housing
    g.setColour (kOuterFill);
    g.fillRoundedRectangle (105.0f, 80.0f, 162.0f, 235.0f, 46.0f);
    g.setColour (kOuterLine);
    g.drawRoundedRectangle (106.0f, 81.0f, 160.0f, 233.0f, 45.0f, 2.0f);

    // inner frame
    g.setColour (kInnerFill);
    g.fillRoundedRectangle (133.0f, 108.0f, 106.0f, 181.0f, 25.0f);
    g.setColour (kInnerLine);
    g.drawRoundedRectangle (133.0f, 108.0f, 106.0f, 181.0f, 25.0f, 2.0f);

    // total compression window
    const Rectangle<float> win (145.0f, 121.0f, 82.0f, 155.0f);
    Path winPath; winPath.addRoundedRectangle (win, 10.0f);
    g.setColour (kWindow);
    g.fillPath (winPath);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (winPath);
        const float h = jlimit (0.0f, 1.0f, -attn / 36.0f) * 155.0f;
        g.setColour (kGrSlate);
        g.fillRect (win.withHeight (h));
        g.setColour (kGrid);
        for (int i = 0; i < 5; ++i) g.fillRect (145.0f, 146.0f + 26.0f * (float) i, 82.0f, 1.0f);
    }

    // labels
    g.setFont (arial (13.5f));
    g.setColour (kLabel);
    g.drawText ("Gate", Rectangle<int> (28, 74, 60, 18), Justification::centred);
    g.drawText ("Gain", Rectangle<int> (282, 74, 60, 18), Justification::centred);
    g.drawText ("Comp", Rectangle<int> (155, 88, 60, 18), Justification::centred);
    g.drawText ("Energy", Rectangle<int> (28, 283, 60, 18), Justification::centred);
    g.drawText ("Output", Rectangle<int> (282, 283, 60, 18), Justification::centred);

    // scales
    g.setFont (arial (11.0f, true));
    g.setColour (kScale);
    for (int i = 0; i < 6; ++i)
    {
        const float y = 145.5f + 23.0f * (float) i;
        g.drawText (String (12 * (i + 1)), Rectangle<float> (20.0f, y - 7.0f, 20.0f, 14.0f), Justification::centredRight);
        g.fillRect (43.0f, y, 4.0f, 1.0f);
        const float cy = 146.0f + 25.8f * (float) i;
        g.drawText (String (6 * (i + 1)), Rectangle<float> (104.0f, cy - 7.0f, 20.0f, 14.0f), Justification::centredRight);
        g.fillRect (126.0f, cy, 4.0f, 1.0f);
    }

    // meters
    auto capsule = [&] (Rectangle<float> r, float frac)
    {
        Path p; p.addRoundedRectangle (r, r.getWidth() * 0.5f);
        g.setColour (kMeterBg);
        g.fillPath (p);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        g.setColour (kMeterFg);
        const float h = jlimit (0.0f, 1.0f, frac) * r.getHeight();
        g.fillRect (r.withTop (r.getBottom() - h));
    };
    capsule ({ 50.0f, 121.0f, 15.0f, 156.0f }, (energy + 80.0f) / 80.0f);
    capsule (vuRect (false).toFloat(), (outL + 36.0f) / 36.0f);
    if (! isMono()) capsule (vuRect (true).toFloat(), (outR + 36.0f) / 36.0f);

    // peak-hold readouts
    g.setColour (kReadout);
    g.setFont (arial (11.0f));
    g.drawText (dbText (energyHold, true), Rectangle<int> (39, 303, 33, 14), Justification::centred);
    g.drawText (dbText (outLHold, true), vuTextRect (false), Justification::centred);
    if (! isMono()) g.drawText (dbText (outRHold, true), vuTextRect (true), Justification::centred);
    g.setFont (arial (13.5f));
    g.drawText (dbText (attnHold), Rectangle<int> (148, 287, 74, 23), Justification::centred);
}
