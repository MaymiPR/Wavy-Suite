#include "WavyRenUI.h"
#include "WavySettings.h"

using namespace juce;

namespace wavy::ren
{
Font arial (float h, bool bold) { return Font (FontOptions ("Arial", h, bold ? Font::bold : Font::plain)); }

String dbText (float v, bool infAtMinus80)
{
    if (infAtMinus80 && v <= -79.95f) return "-Inf";
    return String (v, 1);
}

//==============================================================================
Fader::Fader (Kind k, int thumbOffsetX) : kind (k), offsetX (thumbOffsetX)
{
    setSliderStyle (k == Horizontal ? LinearHorizontal : LinearVertical);
    setTextBoxStyle (TextBoxBelow, false, 40, 16);
    setTextBoxIsEditable (true);
    for (auto* c : getChildren())
        if (auto* l = dynamic_cast<Label*> (c)) l->setEditable (false, true);
    setSliderSnapsToMousePosition (false);
    setNumDecimalPlacesToDisplay (1);
    setScrollWheelEnabled (true);
    setMouseDragSensitivity (150);
    applyFormat();
}

void Fader::applyFormat()
{
    auto f = format;
    const bool neg = negateTyped;
    textFromValueFunction = [f] (double v) { return f (v); };
    valueFromTextFunction = [neg] (const String& s) { const double v = s.trim().getDoubleValue(); return neg ? -std::abs (v) : v; };
    updateText();
}

void Fader::valueChanged() { Slider::valueChanged(); if (kind == Big) resized(); }

// like the reference: click the value (without dragging) or double-click to type; Alt-click resets
void Fader::mouseUp (const MouseEvent& e)
{
    Slider::mouseUp (e);
    if (e.mouseWasClicked() && ! e.mods.isAltDown() && valueTextBounds().contains (e.getPosition()))
        showTextBox();
}

void Fader::mouseDoubleClick (const MouseEvent&) { showTextBox(); }

Rectangle<int> Fader::valueTextBounds() const
{
    for (auto* c : getChildren())
        if (dynamic_cast<Label*> (c) != nullptr) return c->getBounds();
    return {};
}

// only the thumb (and its value text) takes the mouse; clicks on the rest of the track fall through
bool Fader::hitTest (int x, int y)
{
    const juce::Point<int> p (x, y);
    if (valueTextBounds().contains (p)) return true;
    const float pos = (float) getPositionOfValue (getValue());
    if (kind == Big)
        return Rectangle<float> (21.0f, pos - 22.0f, 60.0f, 35.0f).contains (p.toFloat());
    if (kind == Horizontal)
        return p.toFloat().getDistanceFrom ({ pos, 17.5f }) <= 14.0f;
    return p.toFloat().getDistanceFrom ({ 16.5f + (float) offsetX, pos - 3.0f }) <= 14.0f;
}

//==============================================================================
int Look::getSliderThumbRadius (Slider& s)
{
    auto& f = static_cast<Fader&> (s);
    return f.kind == Fader::Big ? 32 : f.kind == Fader::Horizontal ? 16 : 20;
}

Slider::SliderLayout Look::getSliderLayout (Slider& s)
{
    auto& f = static_cast<Fader&> (s);
    Slider::SliderLayout l;
    if (f.kind == Fader::Big)
    {
        l.sliderBounds = { 0, 32, 102, 143 };                      // range of the thumb centre
        const float pos = (float) s.getPositionOfValue (s.getValue());
        l.textBoxBounds = { 26, (int) std::round (pos - 32.0f) + 17, 50, 21 };
    }
    else if (f.kind == Fader::Horizontal)
    {
        l.sliderBounds = { 16, 0, 100, 41 };                       // thumb centre runs along the 100 px track
        l.textBoxBounds = f.textArea;
    }
    else
    {
        l.sliderBounds = { f.offsetX, f.trackTop + 20, 33, 155 };   // range of the thumb centre
        l.textBoxBounds = f.textArea;
    }
    return l;
}

Label* Look::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool big = static_cast<Fader&> (s).kind == Fader::Big;
    l->setFont (arial (big ? 15.5f : 16.0f));
    l->setColour (Label::textColourId, big ? kValue : kReadout);
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

void Look::drawLinearSlider (Graphics& g, int, int, int, int, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    auto& f = static_cast<Fader&> (s);
    const bool down = s.isMouseButtonDown();
    const Colour ring = down ? kRingActive : kRing;
    if (f.kind == Fader::Big)
    {
        const float top = pos - 32.0f, cy = top + 27.0f;
        g.setColour (kRing);
        Rectangle<float> pill (22.0f, top + 11.0f, 58.0f, 33.0f);
        g.fillRect (0.0f, cy - 1.5f, pill.getX() + 4.0f, 3.0f);                      // rails stop at the ring
        g.fillRect (pill.getRight() - 4.0f, cy - 1.5f, 102.0f - pill.getRight() + 4.0f, 3.0f);
        g.drawRoundedRectangle (pill.reduced (1.5f), 15.0f, 3.0f);                  // transparent inside
        auto in = pill.reduced (3.0f);
        if (down) { g.setColour (ring); g.drawRoundedRectangle (in.reduced (0.5f), in.getHeight() * 0.5f, 1.5f); }
    }
    else if (f.kind == Fader::Horizontal)
    {
        const float cx = pos, cy = 17.5f;
        g.setColour (ring);
        g.drawEllipse (cx - 12.5f, cy - 12.5f, 25.0f, 25.0f, 2.0f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRect (cx - 0.5f, cy - 8.5f, 1.0f, 17.0f);
    }
    else
    {
        const float cx = 16.5f + (float) f.offsetX, cy = pos - 3.0f;
        g.setColour (ring);                                                          // ring only, transparent inside
        g.drawEllipse (cx - 12.5f, cy - 12.5f, 25.0f, 25.0f, 2.0f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRect (cx - 8.5f, cy - 0.5f, 17.0f, 1.0f);
    }
}

//==============================================================================
void InSwitch::paintButton (Graphics& g, bool hot, bool down)
{
    // pill measured from the reference: 27 x 21 at (3, 1); light grey when on, dark grey when off
    // as the real plugin renders it: a steel-blue pill (darker when out) with a dark rim and a dark "IN"
    const Rectangle<float> pill (3.0f, 1.0f, 27.0f, 21.0f);
    const bool on = getToggleState();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pill.translated (0.0f, 1.5f), 9.0f);
    g.setColour (Colour (55, 71, 84));
    g.fillRoundedRectangle (pill, 9.0f);
    g.setColour (on ? Colour (129, 166, 195) : Colour (77, 100, 117));
    g.fillRoundedRectangle (pill.reduced (1.5f), 7.5f);
    if (hot || down)
    {
        g.setColour (down ? kRingActive : Colour (0, 174, 239));
        g.drawRoundedRectangle (pill.expanded (1.0f), 10.0f, 1.5f);
    }
    g.setColour (on ? Colour (55, 75, 92) : Colour (40, 52, 62));
    g.setFont (arial (12.0f, true));
    g.drawText ("IN", pill, Justification::centred);
}

//==============================================================================
class Editor::Content : public Component
{
public:
    explicit Content (Editor& e) : ed (e) {}
    void paint (Graphics& g) override { ed.paintContent (g); }
    void mouseDown (const MouseEvent& e) override { ed.contentMouseDown (e); }
    Editor& ed;
};

Editor::Editor (AudioProcessor& p, AudioProcessorValueTreeState& state, UndoManager& u, RangedAudioParameter* mode,
                const String& product, const String& version, int designW, int designH)
    : AudioProcessorEditor (&p), apvts (state), undo (u), W (designW), H (designH),
      contentComp (std::make_unique<Content> (*this)),
      toolbar (std::make_unique<wavy::Toolbar> (u, state, StringArray { "Stereo", "Mono" }, mode, product, version)),
      modeParam (mode)
{
    // never take the keyboard focus: the host keeps its shortcuts (space = play / pause) while the window is open
    setWantsKeyboardFocus (false);
    contentComp->setWantsKeyboardFocus (false);
    addAndMakeVisible (*contentComp);
    contentComp->setBounds (0, 0, W, H);
    toolbar->setWantsKeyboardFocus (false);
    toolbar->setMouseClickGrabsKeyboardFocus (false);
    contentComp->addAndMakeVisible (*toolbar);
    toolbar->setTopLeftPosition (0, 0);
    toolbar->getWindowScale = [this] { return (float) getWidth() / (float) W; };
    toolbar->setWindowScale = [this] (float s) { setSize (roundToInt (W * s), roundToInt (H * s)); };

    // resizable, fixed aspect ratio; the size is remembered with the plugin state
    constrainer.setMinimumSize (W * 3 / 4, H * 3 / 4);
    constrainer.setMaximumSize (W * 3, H * 3);
    constrainer.setFixedAspectRatio ((double) W / H);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const int savedW = (int) state.state.getProperty ("uiWidth", roundToInt (W * wavy::defaultWindowScale()));
    setSize (jlimit (W * 3 / 4, W * 3, savedW), jlimit (H * 3 / 4, H * 3, savedW * H / W));
    startTimerHz (30);
}

Editor::~Editor()
{
    stopTimer();
    sliderAtts.clear(); buttonAtts.clear();
    for (auto* f : faders) f->setLookAndFeel (nullptr);
    owned.clear();
}

Fader& Editor::makeFader (Fader::Kind k, int thumbOffsetX)
{
    owned.push_back (std::make_unique<Fader> (k, thumbOffsetX));
    return static_cast<Fader&> (*owned.back());
}

InSwitch& Editor::makeSwitch()
{
    owned.push_back (std::make_unique<InSwitch>());
    return static_cast<InSwitch&> (*owned.back());
}

void Editor::resized()
{
    contentComp->setTransform (AffineTransform::scale ((float) getWidth() / (float) W));
    apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}

Component& Editor::content() { return *contentComp; }

bool Editor::isMono() const { return modeParam != nullptr && modeParam->getValue() > 0.5f; }

void Editor::attach (Fader& f, const char* paramID, Rectangle<int> bounds, double resetValue)
{
    f.setLookAndFeel (&look);
    f.setWantsKeyboardFocus (false);
    f.setMouseClickGrabsKeyboardFocus (false);
    contentComp->addAndMakeVisible (f);
    f.setBounds (bounds);
    sliderAtts.push_back (std::make_unique<SliderParameterAttachment> (*apvts.getParameter (paramID), f, &undo));
    f.applyFormat();                                      // the attachment installs its own text functions
    f.setDoubleClickReturnValue (true, resetValue);
    faders.push_back (&f);
}

void Editor::attach (Button& b, const char* paramID, Rectangle<int> bounds)
{
    b.setWantsKeyboardFocus (false);
    b.setMouseClickGrabsKeyboardFocus (false);
    contentComp->addAndMakeVisible (b);
    b.setBounds (bounds);
    buttonAtts.push_back (std::make_unique<ButtonParameterAttachment> (*apvts.getParameter (paramID), b, &undo));
}

void Editor::paintPanel (Graphics& g) const
{
    g.fillAll (Colours::black);
    const Rectangle<float> panel (1.0f, 36.0f, (float) W - 2.0f, (float) H - 37.0f);
    g.setColour (kPanel);
    g.fillRect (panel);
    ColourGradient glow (Colour (0, 9, 18).withAlpha (0.55f), 185.0f, 200.0f, Colour (0, 9, 18).withAlpha (0.0f), 185.0f, 360.0f, true);
    glow.addColour (0.55, Colour (0, 9, 18).withAlpha (0.28f));
    g.setGradientFill (glow);
    g.fillRect (panel);
    g.setColour (kFrameLine);
    g.drawRect (panel, 1.0f);
}

void Editor::paintHousing (Graphics& g)
{
    g.setColour (kOuterFill);
    g.fillRoundedRectangle (105.0f, 80.0f, 162.0f, 235.0f, 46.0f);
    g.setColour (kOuterLine);
    g.drawRoundedRectangle (106.0f, 81.0f, 160.0f, 233.0f, 45.0f, 2.0f);
    g.setColour (kInnerFill);
    g.fillRoundedRectangle (133.0f, 108.0f, 106.0f, 181.0f, 25.0f);
    g.setColour (kInnerLine);
    g.drawRoundedRectangle (133.0f, 108.0f, 106.0f, 181.0f, 25.0f, 2.0f);
}

void Editor::paintWindow (Graphics& g, float frac, Colour fill, Colour fillLine, int lines, float firstLineY, float lineStep, bool fromBottom)
{
    const Rectangle<float> win (145.0f, 121.0f, 82.0f, 155.0f);
    Path winPath; winPath.addRoundedRectangle (win, 10.0f);
    g.setColour (kWindow);
    g.fillPath (winPath);
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (winPath);
    const float h = jlimit (0.0f, 1.0f, frac) * win.getHeight();
    const auto filled = fromBottom ? win.withTop (win.getBottom() - h) : win.withHeight (h);
    g.setColour (fill);
    g.fillRect (filled);
    for (int i = 0; i < lines; ++i)
    {
        const float y = firstLineY + lineStep * (float) i;
        g.setColour (filled.contains (150.0f, y) ? fillLine : kGrid);
        g.fillRect (145.0f, std::round (y), 82.0f, 1.0f);
    }
}

void Editor::paintCapsule (Graphics& g, Rectangle<float> r, float frac)
{
    Path p; p.addRoundedRectangle (r, r.getWidth() * 0.5f);
    g.setColour (kMeterBg);
    g.fillPath (p);
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (p);
    g.setColour (kMeterFg);
    const float h = jlimit (0.0f, 1.0f, frac) * r.getHeight();
    g.fillRect (r.withTop (r.getBottom() - h));
}

void Editor::paintTitle (Graphics& g, const String& name, Colour nameColour)
{
    // script W + the plugin's name in red, centred on the panel like the reference's logo
    const auto nameFont = arial (22.0f);
    const float nameW = GlyphArrangement::getStringWidth (nameFont, name);
    const float total = 30.0f + 2.0f + nameW, x0 = 185.5f - total * 0.5f;
    g.setColour (Colour (170, 176, 182));
    g.setFont (Font (FontOptions ("Georgia", 26.0f, Font::italic)));
    g.drawText ("W", Rectangle<float> (x0 - 2.0f, 46.0f, 34.0f, 30.0f), Justification::centred);
    g.setColour (nameColour);
    g.setFont (nameFont);
    g.drawText (name, Rectangle<float> (x0 + 32.0f, 49.0f, nameW + 4.0f, 26.0f), Justification::centredLeft);
}

// the reference's clip light (23 x 13): grey rim, dark inner ring, core grey when off and pink-red when lit
void Editor::paintClipLight (Graphics& g, Rectangle<float> r, bool lit)
{
    const auto outer = Rectangle<float> (r.getX() + 2.0f, r.getY() + 1.0f, 19.0f, 11.0f);
    g.setColour (Colour (119, 119, 119));
    g.fillRoundedRectangle (outer, 5.5f);
    g.setColour (lit ? Colour (67, 67, 67) : Colour (89, 89, 89));
    g.fillRoundedRectangle (outer.reduced (2.0f, 1.0f), 4.0f);
    g.setColour (lit ? Colour (242, 109, 125) : Colour (119, 119, 119));
    g.fillRoundedRectangle (outer.reduced (4.0f, 3.0f), 2.5f);
}
}
