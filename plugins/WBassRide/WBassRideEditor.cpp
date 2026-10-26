#include "WBassRideEditor.h"
#include "WBassRideSkinData.h"

using namespace juce;
using namespace wbassride::ui;
using wavy::ren::arial;

namespace
{
// colours measured on the reference's editor (reference/wbassride, git-ignored)
const Colour kLabelLight (177, 177, 177), kLabelDim (153, 153, 153), kFrame (104, 104, 104), kLabelDark (38, 36, 30),
    kTick (34, 33, 28), kValueBox (0, 0, 0), kValueText (236, 236, 236), kAmberTop (255, 205, 60), kAmberBottom (231, 150, 0),
    kIdleLine (205, 120, 105), kIdleArrow (36, 52, 84), kMarkSelected (241, 227, 7);

// a fitted cap (WBassRideSkinData.h): grey per row x factor per column, alpha per row x per column
void drawCap (Graphics& g, const wbassride::skin::Cap& c, float x, float y, int columns = 1 << 20)
{
    for (int r = 0; r < c.h; ++r)
        for (int k = 0; k < jmin (c.w, columns); ++k)
        {
            const float a = c.alphaRows[r] * c.alphaCols[k];
            if (a < 0.02f) continue;
            const float v = jlimit (0.0f, 255.0f, c.rows[r] * c.cols[k]);
            g.setColour (Colour::fromFloatRGBA (v * c.tint[0] / 255.0f, v * c.tint[1] / 255.0f, v * c.tint[2] / 255.0f, jmin (1.0f, a)));
            g.fillRect (x + (float) k, y + (float) r, 1.0f, 1.0f);
        }
}

// a deterministic grain value in -1 .. 1 for a pixel
float hashNoise (int x, int y, int seed)
{
    uint32 h = (uint32) x * 374761393u + (uint32) y * 668265263u + (uint32) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffff) / 32767.5f - 1.0f;
}

// a value in a reference-style black box (Range, Rider, Output)
void valueBox (Graphics& g, Rectangle<float> r, const String& text)
{
    g.setColour (kValueBox);
    g.fillRect (r);
    g.setColour (kValueText);
    g.setFont (arial (11.0f));
    g.drawText (text, r, Justification::centred);
}

// a selected control's value (measured): a dark box in a 1 px white frame, light grey text
void hoverBox (Graphics& g, Rectangle<float> r, const String& text)
{
    g.setColour (Colour (18, 18, 16));
    g.fillRect (r);
    g.setColour (Colours::white);
    g.drawRect (r, 1.0f);
    g.setColour (Colour (215, 215, 215));
    g.setFont (arial (11.0f));
    g.drawText (text, r, Justification::centred);
}

void setMarks (Marks* m, Component* c, bool hover)
{
    if (m == nullptr) return;
    if (hover) m->hover = c; else if (m->hover == c) m->hover = nullptr;
    if (m->changed) m->changed();
}
}

//==============================================================================
void wbassride::ui::drawMark (Graphics& g, Rectangle<float> r, Mark m, bool round)
{
    if (m == Mark::None) return;
    g.setColour (kMarkSelected);
    if (round) g.drawEllipse (r, 1.5f);
    else g.drawRoundedRectangle (r, 1.5f, 1.2f);
}

//==============================================================================
bool LinearDrag::down (Slider& s, const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return false;
    if (e.mods.isAltDown() && s.isDoubleClickReturnEnabled()) { s.setValue (s.getDoubleClickReturnValue(), sendNotificationSync); return true; }
    active = true;
    start = s.getValue();
    origin = last = virt = e.position;
    s.Slider::mouseDown (e);                                       // the host's gesture begins
    return true;
}

void LinearDrag::drag (Slider& s, const MouseEvent& e, float pxPerUnit, bool vertical)
{
    if (! active) return;
    const auto d = e.position - last;
    last = e.position;
    float k = 1.0f;
    if (wavy::ctrlHeldNow()) k = std::abs (vertical ? d.y : d.x) <= 2.0f ? 0.037f : 0.19f;
    virt += d * k;
    const float moved = vertical ? origin.y - virt.y : virt.x - origin.x;
    s.setValue (jlimit (s.getMinimum(), s.getMaximum(), start + (double) (moved / pxPerUnit)), sendNotificationSync);
}

void LinearDrag::up (Slider& s, const MouseEvent& e)
{
    if (! active) return;
    active = false;
    s.Slider::mouseUp (e);
}

//==============================================================================
Knob::Knob()
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setRotaryParameters (degreesToRadians (228.0f), degreesToRadians (492.0f), true);   // +-132 degrees (measured on the strip)
    setMouseDragSensitivity (500);
    setVelocityModeParameters (1.0, 1, 0.0, false);
}

void Knob::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void Knob::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }
void Knob::mouseDown (const MouseEvent& e)
{
    if (marks != nullptr) { marks->selected = this; if (marks->changed) marks->changed(); }
    if (! feel.down (*this, e, {}, nullptr)) Slider::mouseDown (e);
}
void Knob::mouseDrag (const MouseEvent& e) { if (feel.active) feel.drag (*this, e, nullptr); else Slider::mouseDrag (e); }
void Knob::mouseUp (const MouseEvent& e) { if (! feel.up (*this, e, getLocalBounds().getCentre().toFloat(), nullptr)) Slider::mouseUp (e); }

// the dome as measured on the reference's knob (44 px, shot at its default turn): graded from ~187 at the top to ~121
// at the bottom (linear in y), a bright arc at the top edge (205), a bright reflection near the bottom (216), a black
// line at its edge (radius 19.5 .. 20.5); the pointer a 2 px dark line from 4 to 18 px out
void Knob::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = 20.5f;
    g.setColour (Colour (22, 22, 22));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    const float d = 19.3f;
    ColourGradient dome (Colour (196, 196, 198), c.x, c.y - d, Colour (108, 108, 110), c.x, c.y + d, false);
    dome.addColour (0.5, Colour (152, 152, 154));
    g.setGradientFill (dome);
    g.fillEllipse (c.x - d, c.y - d, 2.0f * d, 2.0f * d);
    // a little darker towards the edge
    ColourGradient edge (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.35f), c.x + d, c.y, true);
    edge.addColour (0.80, Colours::transparentBlack);
    g.setGradientFill (edge);
    g.fillEllipse (c.x - d, c.y - d, 2.0f * d, 2.0f * d);
    Path top;
    top.addCentredArc (c.x, c.y, d - 1.2f, d - 1.2f, 0.0f, degreesToRadians (-50.0f), degreesToRadians (50.0f), true);
    g.setColour (Colour (215, 215, 217).withAlpha (0.9f));
    g.strokePath (top, PathStrokeType (1.6f));
    Path bottom;
    bottom.addCentredArc (c.x, c.y, d - 3.0f, d - 3.0f, 0.0f, degreesToRadians (145.0f), degreesToRadians (215.0f), true);
    g.setColour (Colour (225, 225, 227).withAlpha (0.85f));
    g.strokePath (bottom, PathStrokeType (1.5f));
    // the pointer
    const auto& rp = getRotaryParameters();
    const float a = rp.startAngleRadians + (float) valueToProportionOfLength (getValue()) * (rp.endAngleRadians - rp.startAngleRadians);
    const Point<float> dir (std::sin (a), -std::cos (a));
    g.setColour (Colour (88, 88, 90));
    g.drawLine (Line<float> (c + dir * 4.0f, c + dir * 18.0f), 2.2f);
    drawMark (g, Rectangle<float> (2.0f * d, 2.0f * d).withCentre (c), marks != nullptr ? marks->of (this) : Mark::None, true);
}

//==============================================================================
Fader::Fader (Kind k) : kind (k)
{
    setSliderStyle (LinearVertical);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setVelocityModeParameters (1.0, 1, 0.0, false);
}

// the cap's reference point: Output and the Rider centred on the value; Range Max's cap ends at its value, Range Min's
// starts there (measured: the band between them is the range)
float Fader::yOf (double v) const { return scaleY ((float) v) - (float) getY(); }

Rectangle<float> Fader::capBounds() const
{
    const float y = yOf (getValue());
    switch (kind)
    {
        case Rider:    return { 3.0f, y - 34.0f, 54.0f, 58.0f };
        case Output:   return { 4.0f, y - 9.0f, 38.0f, 18.0f };
        case RangeMax: return { 5.0f, y - 19.0f, 38.0f, 18.0f };
        case RangeMin: return { 5.0f, y + 3.0f, 38.0f, 18.0f };
    }
    return {};
}

bool Fader::hitTest (int x, int y) { return capBounds().expanded (2.0f).contains ((float) x, (float) y); }
void Fader::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void Fader::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }

void Fader::mouseDown (const MouseEvent& e)
{
    if (marks != nullptr) { marks->selected = this; if (marks->changed) marks->changed(); }
    if (kind == Rider && onGrab) onGrab (true);
    feel.down (*this, e);
}
void Fader::mouseDrag (const MouseEvent& e) { feel.drag (*this, e, 8.75f, true); }
void Fader::mouseUp (const MouseEvent& e)
{
    feel.up (*this, e);
    if (kind == Rider && onGrab) onGrab (false);
}

void Fader::paint (Graphics& g)
{
    namespace sk = wbassride::skin;
    const auto cap = capBounds();
    const Mark m = marks != nullptr ? marks->of (this) : Mark::None;
    // the cast shadow (fitted on the reference's): a profile below the cap, thin lines along its sides
    const sk::Cap& img = kind == Rider ? sk::rider : kind == Output ? sk::output : sk::range;
    const Point<float> origin = kind == Rider ? Point<float> (cap.getX() - 2.0f, cap.getY() - 1.0f)
                              : kind == Output ? Point<float> (cap.getX() - 4.0f, cap.getY() - 2.0f) : Point<float> (cap.getX() - 5.0f, cap.getY() - 2.0f);
    const float* prof = kind == Rider ? sk::riderShadow : kind == Output ? sk::outputShadow : sk::rangeShadow;
    const int n = kind == Rider ? (int) std::size (sk::riderShadow) : kind == Output ? (int) std::size (sk::outputShadow) : (int) std::size (sk::rangeShadow);
    const int start = kind == Rider ? sk::riderShadowStart : kind == Output ? sk::outputShadowStart : sk::rangeShadowStart;
    const float side = kind == Rider ? sk::riderShadowSide : kind == Output ? sk::outputShadowSide : sk::rangeShadowSide;
    for (int r = 0; r < n; ++r)
    {
        g.setColour (Colours::black.withAlpha (jlimit (0.0f, 1.0f, prof[r])));
        g.fillRect (origin.x + 2.0f, origin.y + (float) (start + r), (float) img.w - 4.0f, 1.0f);
    }
    g.setColour (Colours::black.withAlpha (side * 0.5f));
    g.fillRect (origin.x + 1.0f, origin.y + 2.0f, 1.0f, (float) start - 2.0f);
    g.fillRect (origin.x + (float) img.w - 2.0f, origin.y + 2.0f, 1.0f, (float) start - 2.0f);
    switch (kind)
    {
        case Rider:    drawCap (g, sk::rider, cap.getX() - 2.0f, cap.getY() - 1.0f); break;
        case Output:   drawCap (g, sk::output, cap.getX() - 4.0f, cap.getY() - 2.0f); break;
        case RangeMax:
        case RangeMin: drawCap (g, sk::range, cap.getX() - 5.0f, cap.getY() - 2.0f); break;
    }
    drawMark (g, cap.expanded (1.0f), m);
}

//==============================================================================
TargetSlider::TargetSlider()
{
    setSliderStyle (LinearHorizontal);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setVelocityModeParameters (1.0, 1, 0.0, false);
}

// the control is 210 px wide (59 .. 269); the thumb 29 px: its left edge from 0 (-42 dB) to 181 (0 dB)
float TargetSlider::thumbX (double v) const { return (float) ((v + 42.0) / 42.0 * 181.0); }
bool TargetSlider::hitTest (int x, int y) { return Rectangle<float> (thumbX (getValue()), 0.0f, 29.0f, 22.0f).expanded (2.0f).contains ((float) x, (float) y); }
void TargetSlider::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void TargetSlider::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }
void TargetSlider::mouseDown (const MouseEvent& e)
{
    if (marks != nullptr) { marks->selected = this; if (marks->changed) marks->changed(); }
    feel.down (*this, e);
}
void TargetSlider::mouseDrag (const MouseEvent& e) { feel.drag (*this, e, 181.0f / 42.0f, false); }
void TargetSlider::mouseUp (const MouseEvent& e) { feel.up (*this, e); }

void TargetSlider::paint (Graphics& g)
{
    // the Energy meter under the thumb: 184 x 15 at 12, 3 (local); -42 .. 0 dBFS from left to right, its track and lit
    // bar fitted on the reference's
    const float lit = jlimit (0.0f, 1.0f, (energyDb + 42.0f) / 42.0f);
    drawCap (g, wbassride::skin::meterTrack, 12.0f, 3.0f);
    if (lit > 0.0f) drawCap (g, wbassride::skin::meterLit, 12.0f, 3.0f, roundToInt (184.0f * lit));
    // the thumb (fitted on the reference's, WBassRideSkinData.h): a 1 px dark frame around a white glass, a red line
    namespace sk = wbassride::skin;
    const Rectangle<float> t (thumbX (getValue()), 0.0f, 29.0f, 22.0f);
    for (int r = 0; r < 2; ++r)
    {
        g.setColour (Colours::black.withAlpha (sk::thumbShadow[r]));
        g.fillRect (t.getX() + 3.0f, t.getY() + 19.0f + (float) r, 23.0f, 1.0f);
    }
    g.setColour (Colour::greyLevel (sk::thumbFrame / 255.0f));
    g.drawRect (t.getX() + 2.0f, t.getY() + 1.0f, 25.0f, 18.0f, 1.0f);
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 23; ++c)
        {
            if (c == 11) continue;                                       // the line's column
            const float al = jlimit (0.0f, 1.0f, sk::thumbGlassAlpha[r] * sk::thumbGlassCol[c]);
            g.setColour (Colour::greyLevel (sk::thumbGlassGrey[r] / 255.0f).withAlpha (al));
            g.fillRect (t.getX() + 3.0f + (float) c, t.getY() + 2.0f + (float) r, 1.0f, 1.0f);
        }
    for (int r = 0; r < 16; ++r)
    {
        const float v = sk::thumbLine[r] / 183.0f;
        g.setColour (Colour::fromFloatRGBA (jmin (1.0f, 0.95f * v + 0.1f), 0.12f * v, 0.12f * v, 1.0f));
        g.fillRect (t.getX() + 14.0f, t.getY() + 2.0f + (float) r, 1.0f, 1.0f);
    }
    drawMark (g, t, marks != nullptr ? marks->of (this) : Mark::None);
}

//==============================================================================
// the switch (37 x 17): a dark slot, the ridged chrome block on the left (Slow) or the right (Fast)
void Switch::paintButton (Graphics& g, bool, bool)
{
    const auto r = getLocalBounds().toFloat();
    ColourGradient slot (Colour (20, 20, 20), 0.0f, 0.0f, Colour (36, 36, 36), 0.0f, r.getHeight(), false);
    g.setGradientFill (slot);
    g.fillRoundedRectangle (r.reduced (0.5f), 2.0f);
    g.setColour (Colour (70, 70, 70));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
    const float bx = getToggleState() ? r.getWidth() - 19.0f : 1.0f;
    drawCap (g, wbassride::skin::switchBlock, bx + 3.0f, 1.0f);
    g.setColour (Colour (150, 150, 150));
    g.fillRect (bx + 1.0f, 1.0f, 2.0f, 15.0f);
    g.fillRect (bx + 16.0f, 1.0f, 2.0f, 15.0f);
    drawMark (g, r, marks != nullptr ? marks->of (this) : Mark::None);
}

//==============================================================================
WBassRideEditor::WBassRideEditor (WBassRideProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wbassride::mode), "WBass Ride", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Renaissance, wbassride::modeNames()),
      proc (p),
      sensitivity (make<Knob>()), spill (make<Knob>()), artifacts (make<Knob>()), responseSwitch (make<Switch>()),
      target (make<TargetSlider>()), rangeMax (make<Fader> (Fader::RangeMax)), rangeMin (make<Fader> (Fader::RangeMin)),
      output (make<Fader> (Fader::Output)), rider (make<Fader> (Fader::Rider))
{
    marks.changed = [this] { content().repaint(); };
    for (auto* k : { &sensitivity, &spill, &artifacts }) k->marks = &marks;
    for (auto* f : { &rangeMax, &rangeMin, &output, &rider }) f->marks = &marks;
    target.marks = &marks;
    responseSwitch.marks = &marks;

    // control rectangles from the reference's GUIXML
    attachSlider (sensitivity, wbassride::sensitivity, { 24, 90, 44, 44 }, 50.0);
    attachSlider (spill, wbassride::spill, { 178, 90, 44, 44 }, -40.0);
    attachSlider (artifacts, wbassride::artifacts, { 253, 90, 44, 44 }, 50.0);
    attach (responseSwitch, wbassride::response, { 104, 106, 37, 17 });
    attachSlider (target, wbassride::target, { 59, 45, 210, 22 }, -14.0);
    attachSlider (rangeMax, wbassride::rangeMax, { 36, 190, 48, 270 }, 12.0);
    attachSlider (rangeMin, wbassride::rangeMin, { 36, 190, 48, 270 }, -12.0);
    attachSlider (output, wbassride::output, { 235, 190, 48, 270 }, 0.0);
    // the Rider: not a parameter; it shows the processor's gain and can be held by hand
    rider.setRange (-12.0, 12.0, 0.01);
    addToContent (rider, { 128, 190, 64, 270 });
    rider.onGrab = [this] (bool held)
    {
        proc.userRideDb.store ((float) rider.getValue());
        proc.userRide.store (held);
    };
    rider.onValueChange = [this] { if (proc.userRide.load()) proc.userRideDb.store ((float) rider.getValue()); };
    // Range Max stays at least 1 dB above Min (the Effective Range is 1 .. 24 dB)
    rangeMax.onValueChange = [this] { if (rangeMin.getValue() > rangeMax.getValue() - 1.0) rangeMin.setValue (rangeMax.getValue() - 1.0, sendNotificationSync); content().repaint(); };
    rangeMin.onValueChange = [this] { if (rangeMax.getValue() < rangeMin.getValue() + 1.0) rangeMax.setValue (rangeMin.getValue() + 1.0, sendNotificationSync); content().repaint(); };
    output.onValueChange = [this] { content().repaint (230, 460, 60, 30); };
}

WBassRideEditor::~WBassRideEditor()
{
    for (auto* k : { &sensitivity, &spill, &artifacts }) k->marks = nullptr;
}

Rectangle<float> WBassRideEditor::idleArrow() const
{
    const float y = scaleY (proc.idleDb());
    return { 35.0f, y - 5.5f, 12.0f, 11.0f };
}

void WBassRideEditor::tick()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001, dt = lastTick > 0.0 ? now - lastTick : 0.0;
    lastTick = now;
    energy = energyBar.update (proc.energyDb.take(), dt);
    target.energyDb = energy;
    target.repaint();
    if (proc.outDb.take() >= 0.0f) { clip = true; clipUntil = now + 1.0; }
    else if (clip && now > clipUntil) clip = false;
    const float r = proc.rideDb.load();
    if (! rider.isMouseButtonDown() && std::abs (r - ride) > 1.0e-4f)
    {
        ride = r;
        rider.setValue (r, dontSendNotification);
    }
    content().repaint (120, 170, 190, 320);
}

void WBassRideEditor::contentMouseDown (const MouseEvent& e)
{
    if (idleArrow().expanded (3.0f).contains (e.position))
    {
        draggingIdle = true;
        idleGrabOffset = e.position.y - idleArrow().getCentreY();
    }
}

void WBassRideEditor::contentMouseDrag (const MouseEvent& e)
{
    if (! draggingIdle) return;
    const float lo = proc.get (wbassride::rangeMin), hi = proc.get (wbassride::rangeMax);
    const float db = jlimit (lo, hi, dbAtY (e.position.y - idleGrabOffset));
    auto* p = proc.apvts.getParameter (wbassride::idle);
    p->setValueNotifyingHost (p->convertTo0to1 (jlimit (0.02f, 0.98f, (db - lo) / jmax (1.0f, hi - lo))));
    content().repaint();
}

void WBassRideEditor::contentMouseUp (const MouseEvent&) { draggingIdle = false; }

//==============================================================================
// the static art, in design coordinates
void WBassRideEditor::paintStatic (Graphics& g) const
{
    // the plug-in's name plate: a bass clef and the name in a serif, silver
    {
        Path clef;
        clef.startNewSubPath (16.0f, 528.0f);
        clef.cubicTo (17.0f, 519.0f, 33.0f, 517.0f, 33.0f, 529.0f);
        clef.cubicTo (33.0f, 539.0f, 25.0f, 545.0f, 16.0f, 549.0f);
        g.setColour (Colour (228, 228, 228));
        g.strokePath (clef, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillEllipse (14.0f, 525.0f, 6.0f, 6.0f);
        g.fillEllipse (36.0f, 522.5f, 3.2f, 3.2f);
        g.fillEllipse (36.0f, 530.5f, 3.2f, 3.2f);
        const Rectangle<float> name (60.0f, 518.0f, 200.0f, 32.0f);
        GlyphArrangement ga;
        ga.addFittedText (Font (FontOptions ("Georgia", 25.0f, Font::bold)), "WBass Ride", name.getX(), name.getY(), name.getWidth(), name.getHeight(), Justification::centred, 1);
        Path text;
        ga.createPath (text);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillPath (text, AffineTransform::translation (1.0f, 1.5f));
        ColourGradient silver (Colour (238, 238, 238), 0.0f, 522.0f, Colour (150, 150, 150), 0.0f, 546.0f, false);
        g.setGradientFill (silver);
        g.fillPath (text);
    }

    // ---- top panel: the group frames (2 px, measured on x 7-8 / 311-312, y 76 and 154-155; joined at x 161)
    g.setColour (kFrame);
    const PathStrokeType frameStroke (1.8f);
    // each group: its title gap in the top line, round outer corners; at the junction (x 161) the top and bottom
    // lines curve into short stems (y 76 .. 86 and 145 .. 155) and the side stays open (measured)
    auto group = [&] (float xOuter, float xJoin, float gapOuter, float gapJoin)    // the title gap's ends
    {
        const float top = 76.5f, bottom = 155.0f, r = 8.0f, j = 5.0f, dir = xJoin > xOuter ? 1.0f : -1.0f;
        Path p;
        p.startNewSubPath (gapOuter, top);
        p.lineTo (xOuter + dir * r, top);
        p.quadraticTo (xOuter, top, xOuter, top + r);
        p.lineTo (xOuter, bottom - r);
        p.quadraticTo (xOuter, bottom, xOuter + dir * r, bottom);
        p.lineTo (xJoin - dir * j, bottom);
        p.quadraticTo (xJoin, bottom, xJoin, bottom - 9.0f);
        g.strokePath (p, frameStroke);
        Path q;
        q.startNewSubPath (gapJoin, top);
        q.lineTo (xJoin - dir * j, top);
        q.quadraticTo (xJoin, top, xJoin, top + 10.0f);
        g.strokePath (q, frameStroke);
    };
    group (8.0f, 161.0f, 58.0f, 105.0f);
    group (312.0f, 161.0f, 262.0f, 214.0f);
    // the panel's darker discs under the knobs
    for (float cx : { 46.0f, 200.0f, 275.0f })
    {
        ColourGradient sh (Colours::black.withAlpha (0.22f), cx, 140.0f, Colours::transparentBlack, cx + 24.0f, 140.0f, true);
        g.setGradientFill (sh);
        g.fillEllipse (cx - 24.0f, 116.0f, 48.0f, 48.0f);
    }

    g.setColour (kLabelLight);
    g.setFont (arial (12.0f, true));
    g.drawText ("Target", Rectangle<float> (13.0f, 48.0f, 50.0f, 14.0f), Justification::centredLeft);
    g.drawText ("Sensitivity", Rectangle<float> (14.0f, 137.0f, 64.0f, 14.0f), Justification::centred);
    g.drawText ("Response", Rectangle<float> (92.0f, 137.0f, 62.0f, 14.0f), Justification::centred);
    g.drawText ("Spill", Rectangle<float> (178.0f, 137.0f, 44.0f, 14.0f), Justification::centred);
    g.drawText ("Artifacts", Rectangle<float> (245.0f, 137.0f, 60.0f, 14.0f), Justification::centred);
    g.setFont (arial (13.5f, true));
    g.drawText ("Detect", Rectangle<float> (58.0f, 68.0f, 47.0f, 16.0f), Justification::centred);
    g.drawText ("Ignore", Rectangle<float> (214.0f, 68.0f, 48.0f, 16.0f), Justification::centred);
    g.setColour (kLabelDim);
    g.setFont (arial (10.0f, true));
    for (float cx : { 46.0f, 200.0f, 275.0f })
    {
        g.drawText ("Lo", Rectangle<float> (cx - 34.0f, 126.0f, 14.0f, 12.0f), Justification::centred);
        g.drawText ("Hi", Rectangle<float> (cx + 20.0f, 126.0f, 14.0f, 12.0f), Justification::centred);
    }
    g.drawText ("Slow", Rectangle<float> (78.0f, 108.0f, 24.0f, 12.0f), Justification::centred);
    g.drawText ("Fast", Rectangle<float> (143.0f, 108.0f, 24.0f, 12.0f), Justification::centred);

    // the Sensitivity knob's dashed ring, the Spill / Artifacts scales (Lo at -135 degrees to Hi at +135, the scale a little wider than the pointer's +-132)
    {
        const Point<float> c (46.0f, 112.0f);
        g.setColour (Colour (138, 138, 138));
        for (int i = 0; i < 12; ++i)
        {
            const float a0 = degreesToRadians (-135.0f + 270.0f * (float) i / 12.0f + 3.0f), a1 = degreesToRadians (-135.0f + 270.0f * (float) (i + 1) / 12.0f - 3.0f);
            Path seg;
            seg.addCentredArc (c.x, c.y, 25.2f, 25.2f, 0.0f, a0, a1, true);
            g.strokePath (seg, PathStrokeType (5.2f));
        }
        // Spill and Artifacts: a thin light ring at radius 21.5 carrying 11 ticks out to 28 (measured)
        for (float cx : { 200.0f, 275.0f })
        {
            g.setColour (Colour (120, 120, 120));
            g.drawEllipse (cx - 21.5f, 90.5f, 43.0f, 43.0f, 1.0f);
            g.setColour (Colour (150, 150, 150));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = degreesToRadians (-135.0f + 27.0f * (float) i);
                const Point<float> d (std::sin (a), -std::cos (a));
                g.drawLine (Line<float> (Point<float> (cx, 112.0f) + d * 21.5f, Point<float> (cx, 112.0f) + d * 28.0f), 2.0f);
            }
        }
    }

    // ---- lower panel: labels, screws, the clip LED's socket, the wells, tracks, scales and value boxes
    g.setColour (kLabelDark);
    g.setFont (arial (12.0f, true));
    g.drawText ("Range", Rectangle<float> (36.0f, 185.0f, 52.0f, 14.0f), Justification::centred);
    g.drawText ("Rider", Rectangle<float> (134.0f, 185.0f, 52.0f, 14.0f), Justification::centred);
    g.drawText ("Output", Rectangle<float> (231.0f, 185.0f, 52.0f, 14.0f), Justification::centred);
    for (float y : { 177.5f, 495.0f })
    {
        g.setColour (Colour (30, 30, 26));
        g.drawEllipse (155.0f, y - 4.5f, 9.0f, 9.0f, 1.3f);
        g.fillEllipse (157.8f, y - 1.7f, 3.4f, 3.4f);
    }
    for (float x : { 52.0f, 150.0f, 248.0f })                       // the wells behind the tracks
    {
        g.setColour (Colours::black.withAlpha (0.07f));
        g.fillRoundedRectangle (x, 200.0f, 20.0f, 256.0f, 5.0f);
    }
    g.setColour (Colour (6, 6, 6));
    g.fillRoundedRectangle (60.0f, 210.0f, 3.0f, 238.0f, 1.5f);         // Range
    g.fillRoundedRectangle (157.0f, 210.0f, 5.0f, 238.0f, 2.5f);        // Rider
    g.fillRoundedRectangle (256.0f, 215.0f, 3.0f, 227.0f, 1.5f);        // Output
    // the Rider's scale on both sides, the Output's on its right: 6 dB marks with numbers, 3 dB marks, 0.5 dB ticks
    g.setFont (arial (11.0f));
    for (int half = -24; half <= 24; ++half)
    {
        const float db = (float) half * 0.5f, y = scaleY (db);
        const bool six = half % 12 == 0, three = half % 6 == 0, thick = std::abs (half) == 24 || half == 0;   // 6 dB: numbered
        const float h = thick ? 2.0f : 1.0f;
        g.setColour (kTick);
        if (three) { g.fillRect (130.0f, y - h * 0.5f, 18.0f, h); g.fillRect (172.0f, y - h * 0.5f, 18.0f, h); }
        else { g.fillRect (140.0f, y - 0.5f, 8.0f, 1.0f); g.fillRect (172.0f, y - 0.5f, 8.0f, 1.0f); }
        if (six)
        {
            const String t = db > 0.0f ? "+" + String ((int) db) : String ((int) db);
            g.setColour (kLabelDark);
            g.drawText (t, Rectangle<float> (104.0f, y - 7.0f, 23.0f, 14.0f), Justification::centredRight);
            g.drawText (t, Rectangle<float> (193.0f, y - 7.0f, 24.0f, 14.0f), Justification::centredLeft);
        }
        // the Output's scale: marks at +12 / 0 / -12, ticks every dB
        g.setColour (kTick);
        if (thick) { g.fillRect (270.0f, y - 1.0f, 10.0f, 2.0f); }
        else g.fillRect (270.0f, y - 0.5f, 6.0f, 1.0f);
        if (thick)
        {
            const String t = db > 0.0f ? "+" + String ((int) db) : String ((int) db);
            g.setColour (kLabelDark);
            g.drawText (t, Rectangle<float> (282.0f, y - 7.0f, 26.0f, 14.0f), Justification::centredLeft);
        }
    }
}

void WBassRideEditor::paintContent (Graphics& g)
{
    namespace sk = wbassride::skin;
    // the static art, rendered once per window scale (the panels from their fitted models, pixel by pixel, then the art)
    const float s = (float) getWidth() / (float) W;
    if (background.isNull() || std::abs (s - backgroundScale) > 1.0e-3f)
    {
        backgroundScale = s;
        const int bw = roundToInt (W * s), bh = roundToInt (H * s);
        background = Image (Image::RGB, bw, bh, true);
        {
            Image::BitmapData bd (background, Image::BitmapData::writeOnly);
            auto panelAt = [] (const sk::Panel& p, float x, float y, float rgb[3])
            {
                const int r = jlimit (0, p.h - 1, (int) (y - (float) p.y0)), c = jlimit (0, p.w - 1, (int) (x - (float) p.x0));
                for (int k = 0; k < 3; ++k) rgb[k] = p.rows[r * 3 + k] * p.cols[c];
            };
            for (int py = 0; py < bh; ++py)
                for (int px = 0; px < bw; ++px)
                {
                    const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                    float rgb[3] = { 0.0f, 0.0f, 0.0f };
                    float grain = 0.0f;
                    if (y >= 36.0f && y < 162.0f)
                    {
                        // the top panel, its edges at x 6 / 313 (one px wider at its foot)
                        if (x >= 6.5f - (y - 36.0f) / 126.0f && x < 314.0f) { panelAt (sk::topPanel, x, y, rgb); grain = 3.2f; }
                    }
                    else if (y >= 162.0f && y < 510.0f)
                    {
                        // the cream panel widens downwards (measured: 5 .. 314 at y 170, 0 .. 318 at 500)
                        const float t = jlimit (0.0f, 1.0f, (y - 166.0f) / 334.0f);
                        if (x >= 5.0f * (1.0f - t) && x < 315.0f + 4.0f * t) { panelAt (sk::creamPanel, x, y, rgb); grain = 2.0f; }
                    }
                    else if (y >= 510.0f && y < 560.0f)
                    {
                        panelAt (sk::leatherPanel, x, y, rgb);
                        grain = 7.0f;
                        // scratches: thin, light, slanted
                        const float u = x * 0.55f + y * 0.9f;
                        const float sc = hashNoise ((int) (u / 3.0f), 7, 3);
                        if (sc > 0.86f) grain += 18.0f * (sc - 0.86f) / 0.14f;
                    }
                    const float n = grain * hashNoise ((int) x, (int) y, 1);
                    bd.setPixelColour (px, py, Colour ((uint8) jlimit (0.0f, 255.0f, rgb[0] + n), (uint8) jlimit (0.0f, 255.0f, rgb[1] + n),
                                                       (uint8) jlimit (0.0f, 255.0f, rgb[2] + n)));
                }
        }
        Graphics bg (background);
        bg.addTransform (AffineTransform::scale (s));
        paintStatic (bg);
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.drawImageAt (background, 0, 0);
    }

    // the Range band: the space between Min and Max, lit, up to the Rider's scale; the Idle line across it
    const float lo = proc.get (wbassride::rangeMin), hi = proc.get (wbassride::rangeMax);
    g.setColour (Colours::white.withAlpha (0.22f));
    g.fillRect (Rectangle<float>::leftTopRightBottom (72.0f, scaleY (hi), 149.0f, scaleY (lo)));
    const auto arrow = idleArrow();
    g.setColour (kIdleLine.withAlpha (0.8f));
    g.fillRect (72.0f, arrow.getCentreY() - 0.5f, 77.0f, 1.0f);
    Path tri;
    tri.addTriangle (arrow.getX() + 2.0f, arrow.getY() + 1.0f, arrow.getX() + 2.0f, arrow.getBottom() - 1.0f, arrow.getRight() - 3.0f, arrow.getCentreY());
    g.setColour (kIdleArrow);
    g.fillPath (tri);
    if (draggingIdle || idleHover) hoverBox (g, { 3.0f, arrow.getCentreY() - 6.5f, 27.0f, 13.0f }, String (proc.idleDb(), 1));

    // the clip LED
    {
        const Rectangle<float> led (280.0f, 184.0f, 15.0f, 12.0f);
        ColourGradient lg (clip ? Colour (255, 70, 60) : Colour (96, 96, 96), 0.0f, led.getY(), clip ? Colour (190, 10, 10) : Colour (52, 52, 52), 0.0f, led.getBottom(), false);
        g.setGradientFill (lg);
        g.fillRoundedRectangle (led, 2.0f);
        g.setColour (Colour (28, 28, 28));
        g.drawRoundedRectangle (led.expanded (0.5f), 2.0f, 1.0f);
    }

    // value boxes
    valueBox (g, { 44.0f, 469.0f, 34.0f, 14.0f }, String (hi - lo, 1));
    valueBox (g, { 143.0f, 469.0f, 34.0f, 14.0f }, String (rider.getValue(), 2));
    valueBox (g, { 241.0f, 469.0f, 34.0f, 14.0f }, String (output.getValue(), 1));

    // the values shown under the mouse (the reference's hover texts)
    if (target.isMouseButtonDown() || marks.showsValue (&target)) hoverBox (g, { 272.0f, 48.0f, 32.0f, 13.0f }, String (target.getValue(), 1));
    // Range Max / Min: the value beside the thumb (measured: a box right of the cap, at its centre)
    for (auto* f : { &rangeMax, &rangeMin })
        if (f->isMouseButtonDown() || marks.showsValue (f))
        {
            const auto cap = f->capBounds().translated ((float) f->getX(), (float) f->getY());
            hoverBox (g, { 88.0f, cap.getCentreY() - 7.0f, 31.0f, 14.0f }, String (f->getValue(), 1));
        }
    const std::pair<Knob*, float> knobs[] = { { &sensitivity, 27.0f }, { &spill, 181.0f }, { &artifacts, 256.0f } };
    for (auto [k, x] : knobs)
        if (k->showing()) hoverBox (g, { x, 157.0f, 37.0f, 13.0f }, String (k->getValue(), 1));

    // the Live components' badge
    if (wbassride::isLive (proc.modeIndex()))
    {
        const Rectangle<float> b (246.0f, 520.0f, 65.0f, 30.0f);
        g.setColour (Colour (24, 24, 24));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Colour (120, 120, 120));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (Colour (250, 215, 30));
        g.setFont (arial (13.0f, true));
        g.drawText ("LIVE", b.withHeight (16.0f).translated (0.0f, 1.0f), Justification::centred);
        g.setColour (Colour (230, 230, 230));
        g.setFont (arial (10.0f, true));
        g.drawText ("Low Latency", b.withTrimmedTop (15.0f), Justification::centred);
    }
}
