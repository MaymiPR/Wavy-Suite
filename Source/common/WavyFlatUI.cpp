#include "WavyFlatUI.h"
using namespace juce;

namespace wavy::flat
{
Font font (float h, bool bold) { return Font (FontOptions ("Arial", h, bold ? Font::bold : Font::plain)); }

float gainDbFromNorm (float v)
{
    if (v <= 0.0f) return -100.0f;
    if (v <= 2.0f / 3.0f) return std::max (-100.0f, 40.0f * std::log10 (1.5f * v));
    const float u = 3.0f * v - 2.0f;
    return 8.0f * u + 4.0f * u * u;
}

float gainNormFromDb (float db)
{
    if (db <= -99.95f) return 0.0f;
    if (db <= 0.0f) return std::pow (10.0f, db / 40.0f) / 1.5f;
    const float u = -1.0f + std::sqrt (1.0f + db / 4.0f);
    return (u + 2.0f) / 3.0f;
}

NormalisableRange<float> gainRange (float maxDb)
{
    const float top = gainNormFromDb (maxDb);           // 2/3 for a 0 dB fader, 1 for +12 dB
    return NormalisableRange<float> (-100.0f, maxDb,
        [top] (float, float, float n) { return gainDbFromNorm (n * top); },
        [top] (float, float, float db) { return jlimit (0.0f, 1.0f, gainNormFromDb (db) / top); },
        [] (float, float, float db) { return db <= -99.95f ? -100.0f : std::round (db * 10.0f) / 10.0f; });
}

String gainText (float db) { return db <= -99.95f ? String ("-inf") : String (db, 1); }

void applyFormat (Slider& s, std::function<String (double)> format)
{
    s.textFromValueFunction = format;
    s.valueFromTextFunction = [] (const String& t)
    {
        const auto x = t.trim();
        return x.containsIgnoreCase ("inf") ? -100.0 : x.getDoubleValue();
    };
    s.updateText();
}

//==============================================================================
static void clickToType (Slider& s, Rectangle<int> textArea, const MouseEvent& e)
{
    if (e.mouseWasClicked() && ! e.mods.isAltDown() && textArea.contains (e.getPosition())) s.showTextBox();
}

VFader::VFader (Colour line) : lineColour (line)
{
    setSliderStyle (LinearVertical);
    setTextBoxStyle (TextBoxBelow, false, 31, 18);
    setSliderSnapsToMousePosition (false);
    setMouseDragSensitivity (165);
    setScrollWheelEnabled (true);
}

bool VFader::hitTest (int x, int y)
{
    const float pos = (float) getPositionOfValue (getValue());
    return textArea.contains (x, y) || Rectangle<float> (0.0f, pos - 16.0f, (float) getWidth(), 32.0f).contains ((float) x, (float) y);
}

void VFader::mouseUp (const MouseEvent& e) { Slider::mouseUp (e); clickToType (*this, textArea, e); }

void VFader::paint (Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f;
    g.setColour (kTrack);
    g.fillRect (cx - 1.0f, (float) travelTop, 2.0f, (float) (travelBottom - travelTop));
    const float pos = (float) getPositionOfValue (getValue());
    const Rectangle<float> thumb (cx - 8.5f, pos - 14.0f, 17.0f, 28.0f);
    for (int k = 3; k >= 1; --k)                                                  // soft shadow
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f).expanded ((float) k), 3.0f + (float) k);
    }
    ColourGradient grad (Colour (66, 67, 70), thumb.getX(), thumb.getY(), Colour (48, 49, 52), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 2.5f);
    g.setColour (lineColour.withAlpha (0.35f));
    g.fillRoundedRectangle (cx - 7.0f, pos - 2.0f, 14.0f, 4.0f, 1.5f);
    g.setColour (lineColour);
    g.fillRoundedRectangle (cx - 6.0f, pos - 1.0f, 12.0f, 2.0f, 1.0f);
}

//==============================================================================
Knob::Knob (String l) : label (std::move (l))
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (TextBoxBelow, false, 54, 14);
    setMouseDragSensitivity (200);
    setScrollWheelEnabled (true);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
}

void Knob::mouseUp (const MouseEvent& e) { Slider::mouseUp (e); clickToType (*this, textArea, e); }

void Knob::paint (Graphics& g)
{
    const auto c = centre;
    g.setColour (kRing.withAlpha (0.9f));
    g.fillEllipse (c.x - 1.3f, 9.5f, 2.6f, 2.6f);                                 // centre-position dot
    g.setColour (kRing);
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

    const auto rp = getRotaryParameters();
    const float frac = (float) valueToProportionOfLength (getValue());
    const float a0 = rp.startAngleRadians, a = a0 + frac * (rp.endAngleRadians - rp.startAngleRadians);
    if (frac > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a, true);
        for (int k = 3; k >= 1; --k)                                               // glow
        {
            g.setColour (kWhite.withAlpha (0.07f));
            g.strokePath (arc, PathStrokeType (2.0f + 2.0f * (float) k, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (kWhite);
        g.strokePath (arc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    const Point<float> dir (std::sin (a), -std::cos (a));
    g.setColour (kWhite);
    g.drawLine (Line<float> (c + dir * radius, c + dir * (radius - 5.0f)), 2.2f);

    g.setColour (kLabel);
    g.setFont (font (13.0f));
    g.drawText (label, Rectangle<int> (0, 52, getWidth(), 16), Justification::centred);
    g.setColour (Colour (82, 83, 86));
    g.fillRect (2.0f, 68.0f, (float) getWidth() - 4.0f, 1.5f);
}

//==============================================================================
HSlider::HSlider()
{
    setSliderStyle (LinearHorizontal);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setMouseDragSensitivity (264);
    setScrollWheelEnabled (true);
}

void HSlider::paint (Graphics& g)
{
    g.setColour (Colour (150, 150, 152));
    for (float t : tickPositions)
        g.fillRect (std::round (x0 + t * (x1 - x0)), y - 13.0f, 1.0f, 8.0f);
    const float pos = (float) getPositionOfValue (getValue());
    g.setColour (kTrack);
    g.fillRoundedRectangle (x0, y - 2.0f, x1 - x0, 4.0f, 2.0f);
    g.setColour (kWhite);
    g.fillRoundedRectangle (x0, y - 1.5f, pos - x0, 3.0f, 1.5f);
    g.setColour (Colour (30, 31, 34));
    g.fillEllipse (pos - 5.0f, y - 5.0f, 10.0f, 10.0f);
    g.setColour (kWhite);
    g.drawEllipse (pos - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.6f);
    if ((isMouseOverOrDragging() || isMouseButtonDown()) && format && ! textArea.isEmpty())
    {
        g.setColour (kValue);
        g.setFont (font (12.0f));
        g.drawText (format (getValue()), textArea, Justification::centredLeft);
    }
}

//==============================================================================
void drawSlopeIcon (Graphics& g, Rectangle<float> r, int slope)
{
    Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    if (slope == 0)        { p.startNewSubPath (x, y + h); p.lineTo (x + w * 0.55f, y + h * 0.05f); p.lineTo (x + w, y); }
    else if (slope == 1)   { p.startNewSubPath (x, y + h); p.quadraticTo (x + w * 0.22f, y + h * 0.05f, x + w * 0.45f, y); p.lineTo (x + w, y); }
    else                   { p.startNewSubPath (x + w * 0.05f, y + h); p.quadraticTo (x + w * 0.15f, y + h * 0.1f, x + w * 0.28f, y); p.lineTo (x + w, y); }
    g.setColour (kWhite.withAlpha (0.15f));
    g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (kWhite);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void SlopeBox::mouseDown (const MouseEvent& e)
{
    stepping = e.x < getWidth() - arrowWidth && getNumItems() > 0;
    if (stepping)
        setSelectedItemIndex ((getSelectedItemIndex() + 1) % getNumItems(), sendNotificationSync);
    else
        ComboBox::mouseDown (e);
}

void SlopeBox::mouseUp (const MouseEvent& e)
{
    if (! stepping) ComboBox::mouseUp (e);
    stepping = false;
}

void SlopeBox::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colour (38, 39, 42));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (isMouseOver() ? Colour (110, 111, 114) : Colour (80, 81, 84));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    drawSlopeIcon (g, { 8.5f, 5.5f, 15.0f, 10.0f }, jmax (0, getSelectedItemIndex()));
    Path tri;
    const float ax = r.getRight() - 15.0f, ay = r.getCentreY();
    tri.addTriangle (ax - 3.0f, ay - 2.0f, ax + 3.0f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (kWhite);
    g.fillPath (tri);
}

//==============================================================================
RadioColumn::RadioColumn (RangedAudioParameter& p, UndoManager* um, StringArray l, std::vector<Colour> a)
    : labels (std::move (l)), accents (std::move (a))
{
    attachment = std::make_unique<ParameterAttachment> (p, [this] (float v) { selected = roundToInt (v); repaint(); }, um);
    attachment->sendInitialUpdate();
}

void RadioColumn::paint (Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        const Rectangle<float> r (0.5f, (float) (i * rowHeight) + 0.5f, (float) getWidth() - 1.0f, (float) buttonHeight - 1.0f);
        const bool on = i == selected;
        const auto acc = accents[(size_t) i];
        if (on)
        {
            g.setColour (acc.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (1.0f), 4.5f, 2.0f);                // glow
            g.setColour (acc.interpolatedWith (kPanel, 0.62f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (acc);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        else
        {
            g.setColour (Colour (58, 59, 62));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (on ? acc.interpolatedWith (Colours::white, 0.35f) : Colour (128, 129, 132));
        g.setFont (font (12.0f));
        g.drawText (labels[i], r, Justification::centred);
    }
}

void RadioColumn::mouseDown (const MouseEvent& e)
{
    const int i = e.y / rowHeight;
    if (i >= 0 && i < labels.size() && e.y - i * rowHeight < buttonHeight)
        attachment->setValueAsCompleteGesture ((float) i);
}

//==============================================================================
Slider::SliderLayout FlatLook::getSliderLayout (Slider& s)
{
    Slider::SliderLayout l;
    if (auto* f = dynamic_cast<VFader*> (&s))
    {
        l.sliderBounds = { 0, f->travelTop, s.getWidth(), f->travelBottom - f->travelTop };
        l.textBoxBounds = f->textArea;
    }
    else if (auto* k = dynamic_cast<Knob*> (&s))
    {
        l.sliderBounds = s.getLocalBounds();
        l.textBoxBounds = k->textArea;
    }
    else if (auto* h = dynamic_cast<HSlider*> (&s))
        l.sliderBounds = { (int) h->x0, 0, (int) (h->x1 - h->x0), s.getHeight() };
    else
        l = LookAndFeel_V4::getSliderLayout (s);
    return l;
}

Label* FlatLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.5f));
    l->setColour (Label::textColourId, kValue);
    l->setColour (Label::textWhenEditingColourId, Colours::white);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (TextEditor::backgroundColourId, Colour (20, 20, 22));
    l->setColour (TextEditor::textColourId, Colours::white);
    l->setColour (TextEditor::highlightColourId, kBlue.withAlpha (0.5f));
    l->setBorderSize ({ 0, 0, 0, 0 });
    l->setJustificationType (Justification::centred);
    l->setInterceptsMouseClicks (false, false);
    return l;
}

PopupMenu::Options FlatLook::getOptionsForComboBoxPopupMenu (ComboBox& box, Label& label)
{
    return LookAndFeel_V4::getOptionsForComboBoxPopupMenu (box, label).withStandardItemHeight (20).withMinimumWidth (130);
}

void FlatLook::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (30, 31, 34));
    g.setColour (Colour (70, 71, 74));
    g.drawRect (0, 0, w, h, 1);
}

void FlatLook::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                  bool isTicked, bool, const String& text, const String&, const Drawable*, const Colour*)
{
    if (isSeparator) { g.setColour (Colour (70, 71, 74)); g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY())); return; }
    if (isHighlighted && isActive) { g.setColour (Colour (62, 63, 67)); g.fillRect (area); }
    if (isTicked)
    {
        Path tick;
        const float cx = (float) area.getX() + 12.0f, cy = (float) area.getCentreY();
        tick.startNewSubPath (cx - 4.0f, cy); tick.lineTo (cx - 1.0f, cy + 3.0f); tick.lineTo (cx + 4.0f, cy - 3.5f);
        g.setColour (kWhite);
        g.strokePath (tick, PathStrokeType (1.6f));
    }
    auto textArea = area.withTrimmedLeft (24);
    if (text.endsWith ("dB/oct"))                  // slope entries show their picture, as in the reference
    {
        const int v = text.getIntValue();
        drawSlopeIcon (g, Rectangle<float> ((float) textArea.getX(), (float) area.getCentreY() - 4.5f, 13.0f, 9.0f),
                       v <= 6 ? 0 : v <= 12 ? 1 : 2);
        textArea = textArea.withTrimmedLeft (20);
    }
    g.setColour (isActive ? Colour (225, 225, 227) : Colour (110, 110, 112));
    g.setFont (font (13.0f));
    g.drawText (text, textArea, Justification::centredLeft);
}

//==============================================================================
void drawMeterBar (Graphics& g, Rectangle<float> t, float frac)
{
    g.setColour (kTrack);
    g.fillRoundedRectangle (t, t.getWidth() * 0.5f);
    frac = jlimit (0.0f, 1.0f, frac);
    if (frac <= 0.0f) return;
    const auto bar = t.withTop (t.getBottom() - t.getHeight() * frac);
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (kWhite.withAlpha (0.08f));
        g.fillRoundedRectangle (bar.expanded ((float) k), (t.getWidth() + 2.0f * (float) k) * 0.5f);
    }
    g.setColour (kWhite);
    g.fillRoundedRectangle (bar, t.getWidth() * 0.5f);
}

void drawBand (Graphics& g, const std::vector<Point<float>>& pts, float bottomY, Colour c)
{
    if (pts.size() < 2) return;
    Path edge, fill;
    edge.startNewSubPath (pts.front());
    for (size_t i = 1; i < pts.size(); ++i) edge.lineTo (pts[i]);
    fill = edge;
    fill.lineTo (pts.back().x, bottomY);
    fill.lineTo (pts.front().x, bottomY);
    fill.closeSubPath();
    float top = bottomY;
    for (auto& p : pts) top = std::min (top, p.y);
    ColourGradient grad (c.withAlpha (0.62f), 0.0f, top, c.withAlpha (0.28f), 0.0f, bottomY, false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (c.withAlpha (0.3f));
    g.strokePath (edge, PathStrokeType (3.0f));
    g.setColour (c.brighter (0.15f));
    g.strokePath (edge, PathStrokeType (1.4f));
}
}
