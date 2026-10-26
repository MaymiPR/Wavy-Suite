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
    kIdleLine (205, 120, 105), kIdleArrow (36, 52, 84), kMarkSelected (241, 227, 7), kMarkMember (7, 150, 229);

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
    // a black box with round corners in a light rim (measured)
    g.setColour (Colour (232, 228, 190).withAlpha (0.7f));
    g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 1.0f);
    g.setColour (kValueBox);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (kValueText);
    g.setFont (arial (12.0f));
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
    g.setColour (m == Mark::Member ? kMarkMember : kMarkSelected);
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
    e.source.enableUnboundedMouseMovement (true, false);         // hidden cursor, no screen edge
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
    this->vertical = vertical;
    this->pxPerUnit = pxPerUnit;
    const float moved = vertical ? origin.y - virt.y : virt.x - origin.x;
    s.setValue (jlimit (s.getMinimum(), s.getMaximum(), start + (double) (moved / pxPerUnit)), sendNotificationSync);
}

void LinearDrag::up (Slider& s, const MouseEvent& e)
{
    if (! active) return;
    active = false;
    s.Slider::mouseUp (e);
    e.source.enableUnboundedMouseMovement (false);
    const float moved = (float) (s.getValue() - start) * pxPerUnit;   // the cursor comes back where the thumb went
    const auto at = vertical ? juce::Point<float> (origin.x, origin.y - moved) : juce::Point<float> (origin.x + moved, origin.y);
    juce::Desktop::setMousePosition (s.localPointToGlobal (at).roundToInt());
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
    blocked = marks != nullptr && ! marks->click (this);
    if (blocked) return;
    if (! feel.down (*this, e, {}, nullptr)) Slider::mouseDown (e);
}
void Knob::mouseDrag (const MouseEvent& e) { if (blocked) return; if (feel.active) feel.drag (*this, e, nullptr); else Slider::mouseDrag (e); }
void Knob::mouseUp (const MouseEvent& e)
{
    if (blocked) { blocked = false; return; }
    if (! feel.up (*this, e, getLocalBounds().getCentre().toFloat(), nullptr)) Slider::mouseUp (e);
}

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

// the cap as the reference draws it (WBassRideSkinData.h, fitted on its 1x frame): outline and lip included
static const wbassride::skin::FrameCap& frameCap (Fader::Kind k)
{
    namespace sk = wbassride::skin;
    return k == Fader::Rider ? sk::riderFrame : k == Fader::Output ? sk::outputFrame : k == Fader::RangeMax ? sk::rangeMaxFrame : sk::rangeMinFrame;
}

Rectangle<float> Fader::capBounds() const
{
    const auto& c = frameCap (kind);
    return { (float) (c.x0 - getX()), yOf (getValue()) + (float) c.dy, (float) c.w, (float) c.h };
}

bool Fader::hitTest (int x, int y) { return capBounds().expanded (2.0f).contains ((float) x, (float) y); }
void Fader::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void Fader::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }

void Fader::mouseDown (const MouseEvent& e)
{
    blocked = marks != nullptr && ! marks->click (this);
    if (blocked) return;
    if (kind == Rider && onGrab) onGrab (true);
    feel.down (*this, e);
}
void Fader::mouseDrag (const MouseEvent& e) { if (! blocked) feel.drag (*this, e, 8.75f, true); }
void Fader::mouseUp (const MouseEvent& e)
{
    if (blocked) { blocked = false; return; }
    feel.up (*this, e);
    if (kind == Rider && onGrab) onGrab (false);
}

void Fader::paint (Graphics& g)
{
    namespace sk = wbassride::skin;
    const auto cap = capBounds();
    const auto& fc = frameCap (kind);
    // the cast shadow below the cap's lip (the skin's profile, after the rows the frame's cap already holds)
    const float* prof = kind == Rider ? sk::riderShadow : kind == Output ? sk::outputShadow : sk::rangeShadow;
    const int n = kind == Rider ? (int) std::size (sk::riderShadow) : kind == Output ? (int) std::size (sk::outputShadow) : (int) std::size (sk::rangeShadow);
    const int skip = kind == Rider ? 3 : 2;
    for (int r = skip; r < n; ++r)
    {
        g.setColour (Colours::black.withAlpha (jlimit (0.0f, 1.0f, prof[r])));
        g.fillRect (cap.getX() + 1.0f, cap.getBottom() + (float) (r - skip), cap.getWidth() - 2.0f, 1.0f);
    }
    {
        Graphics::ScopedSaveState clip (g);
        Path round;
        round.addRoundedRectangle (cap, kind == Rider ? 3.5f : 2.5f);
        g.reduceClipRegion (round);
        for (int r = 0; r < fc.h; ++r)
            for (int k = 0; k < fc.w; ++k)
            {
                const float f = fc.cols[k];
                g.setColour (Colour ((uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3] * f), (uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3 + 1] * f),
                                     (uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3 + 2] * f)));
                g.fillRect (cap.getX() + (float) k, cap.getY() + (float) r, 1.0f, 1.0f);
            }
    }
    drawMark (g, cap, marks != nullptr ? marks->of (this) : Mark::None);
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
    blocked = marks != nullptr && ! marks->click (this);
    if (! blocked) feel.down (*this, e);
}
void TargetSlider::mouseDrag (const MouseEvent& e) { if (! blocked) feel.drag (*this, e, 181.0f / 42.0f, false); }
void TargetSlider::mouseUp (const MouseEvent& e) { if (! blocked) feel.up (*this, e); blocked = false; }

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
void IdleColumn::paint (Graphics& g)
{
    const float y = scaleY (idleDb ? idleDb() : 0.0f) - (float) getY();
    Path tri;
    tri.addTriangle (4.0f, y - 4.5f, 4.0f, y + 4.5f, 11.0f, y);
    g.setColour (kIdleArrow);
    g.fillPath (tri);
    const Mark m = marks != nullptr ? marks->of (this) : Mark::None;
    if (m != Mark::None)
    {
        g.setColour (m == Mark::Member ? kMarkMember : kMarkSelected);
        g.strokePath (tri, PathStrokeType (1.0f));
    }
}

void IdleColumn::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void IdleColumn::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }

void IdleColumn::mouseDown (const MouseEvent& e)
{
    blocked = marks != nullptr && ! marks->click (this);
    if (blocked) return;
    if (e.mods.isAltDown()) { if (reset) reset(); return; }
    dragging = true;
    grab = e.position.y + (float) getY() - scaleY (idleDb());
    e.source.enableUnboundedMouseMovement (true, false);
    if (gesture) gesture (true);
}

void IdleColumn::mouseDrag (const MouseEvent& e)
{
    if (dragging && setIdleDb) setIdleDb (dbAtY (e.position.y + (float) getY() - grab));
    repaint();
}

void IdleColumn::mouseUp (const MouseEvent& e)
{
    blocked = false;
    if (! dragging) return;
    dragging = false;
    if (gesture) gesture (false);
    e.source.enableUnboundedMouseMovement (false);
    Desktop::setMousePosition (localPointToGlobal (Point<float> (e.mouseDownPosition.x, scaleY (idleDb()) - (float) getY() + grab)).roundToInt());
    if (marks != nullptr && marks->changed) marks->changed();
}

//==============================================================================
WBassRideEditor::WBassRideEditor (WBassRideProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wbassride::mode), "WBass Ride", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Renaissance, wbassride::modeNames()),
      proc (p),
      sensitivity (make<Knob>()), spill (make<Knob>()), artifacts (make<Knob>()), responseSwitch (make<Switch>()),
      target (make<TargetSlider>()), rangeMax (make<Fader> (Fader::RangeMax)), rangeMin (make<Fader> (Fader::RangeMin)),
      output (make<Fader> (Fader::Output)), rider (make<Fader> (Fader::Rider)), idleColumn (make<IdleColumn>()),
      lasso (make<wavy::flat::LassoOverlay>())
{
    marks.blueAlone = { &rider, &idleColumn };
    marks.changed = [this] { content().repaint(); };
    marks.enter = [this] (Slider* sl) { openEntry (sl); };
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
        if (! held)                                                 // riding resumes at once (measured)
        {
            ride = proc.idleDb();
            rider.setValue (ride, dontSendNotification);
            content().repaint();
        }
    };
    rider.onValueChange = [this] { if (proc.userRide.load()) proc.userRideDb.store ((float) rider.getValue()); };
    // Range Max stays at least 1 dB above Min (the Effective Range is 1 .. 24 dB)
    rangeMax.onValueChange = [this] { if (rangeMin.getValue() > rangeMax.getValue() - 1.0) rangeMin.setValue (rangeMax.getValue() - 1.0, sendNotificationSync); content().repaint(); };
    rangeMin.onValueChange = [this] { if (rangeMax.getValue() < rangeMin.getValue() + 1.0) rangeMax.setValue (rangeMin.getValue() + 1.0, sendNotificationSync); content().repaint(); };
    output.onValueChange = [this] { content().repaint (230, 460, 60, 30); };
    // the Idle arrow
    auto* idleParam = p.apvts.getParameter (wbassride::idle);
    idleColumn.marks = &marks;
    idleColumn.idleDb = [this] { return proc.idleDb(); };
    idleColumn.setIdleDb = [this, idleParam] (float db)
    {
        const float lo = proc.get (wbassride::rangeMin), hi = proc.get (wbassride::rangeMax);
        idleParam->setValueNotifyingHost (idleParam->convertTo0to1 (jlimit (0.02f, 0.98f, (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo))));
        content().repaint();
    };
    idleColumn.gesture = [idleParam] (bool begin) { if (begin) idleParam->beginChangeGesture(); else idleParam->endChangeGesture(); };
    idleColumn.reset = [this, idleParam] { idleParam->setValueNotifyingHost (idleParam->getDefaultValue()); content().repaint(); };
    addToContent (idleColumn, { 33, 214, 16, 226 });
    addToContent (lasso, { 0, 0, W, H });                          // over everything, takes no clicks
    lasso.setVisible (false);
}

WBassRideEditor::~WBassRideEditor()
{
    entry.reset();
    for (auto* k : { &sensitivity, &spill, &artifacts }) k->marks = nullptr;
}

Rectangle<int> WBassRideEditor::entryArea (Slider* sl) const
{
    if (sl == &sensitivity) return { 27, 157, 37, 13 };
    if (sl == &spill) return { 181, 157, 37, 13 };
    if (sl == &artifacts) return { 256, 157, 37, 13 };
    if (sl == &target) return { 272, 48, 32, 13 };
    if (sl == &output) return { 241, 469, 34, 14 };
    if (sl == &rangeMax || sl == &rangeMin)
    {
        auto* f = static_cast<Fader*> (sl);
        const auto cap = f->capBounds().translated ((float) f->getX(), (float) f->getY());
        return { 88, roundToInt (cap.getCentreY()) - 7, 31, 14 };
    }
    return {};
}

void WBassRideEditor::openEntry (Slider* sl)
{
    const auto area = entryArea (sl);
    if (area.isEmpty()) return;
    closeEntry (false);
    entryFor = sl;
    entry = std::make_unique<TextEditor>();
    entry->setFont (arial (11.0f));
    entry->setJustification (Justification::centredLeft);
    entry->setIndents (2, 0);
    entry->setColour (TextEditor::backgroundColourId, Colours::white);
    entry->setColour (TextEditor::textColourId, Colours::black);
    entry->setColour (TextEditor::outlineColourId, Colours::black);
    entry->setColour (TextEditor::focusedOutlineColourId, Colours::black);
    entry->setText (sl->getTextFromValue (sl->getValue()).upToFirstOccurrenceOf (" ", false, false), false);
    entry->onReturnKey = [this] { closeEntry (true); };
    entry->onEscapeKey = [this] { closeEntry (false); };
    entry->onFocusLost = [this] { closeEntry (false); };
    content().addAndMakeVisible (*entry);
    entry->setBounds (area);
    entry->selectAll();
    entry->grabKeyboardFocus();                                    // only while typing: the host keeps it otherwise
}

void WBassRideEditor::closeEntry (bool apply)
{
    if (entry == nullptr) return;
    auto e = std::move (entry);                                    // (losing the focus calls back here)
    auto* sl = entryFor;
    entryFor = nullptr;
    if (apply && sl != nullptr)
    {
        const auto t = e->getText().trim().upToFirstOccurrenceOf (" ", false, false);
        if (t.containsAnyOf ("0123456789"))
        {
            double v = t.getDoubleValue();
            if (sl == &spill) v = -std::abs (v);                   // the reference's PositiveTextEntryToNegative
            if (v >= sl->getMinimum() - 1.0e-9 && v <= sl->getMaximum() + 1.0e-9)
            {
                Slider::ScopedDragNotification drag (*sl);
                sl->setValue (v, sendNotificationSync);
            }
        }
    }
    e->setVisible (false);
    MessageManager::callAsync ([e = std::shared_ptr<TextEditor> (std::move (e))] {});   // destroyed after its callback returns
    content().repaint();
}

// the lasso (measured on the reference: a 1 px white rectangle while dragging on the background; the controls it
// touches, in the reference's control order: Spill, Rider, Range Max, Range Min, Output, Sensitivity, Target, Idle,
// Response, Artifacts)
void WBassRideEditor::contentMouseDown (const MouseEvent& e)
{
    lassoing = true;
    lassoStart = e.position;
    marks.group.clear();
    marks.selected = nullptr;
    if (marks.changed) marks.changed();
}

void WBassRideEditor::contentMouseDrag (const MouseEvent& e)
{
    if (! lassoing) return;
    const auto r = Rectangle<float> (lassoStart, e.position);
    lasso.rect = r.toNearestInt();
    lasso.setVisible (true);
    lasso.repaint();
    auto at = [] (Component& c, Rectangle<float> local) { return local.translated ((float) c.getX(), (float) c.getY()); };
    const std::pair<Component*, Rectangle<float>> items[] = {
        { &spill, spill.getBounds().toFloat() }, { &rider, at (rider, rider.capBounds()) },
        { &rangeMax, at (rangeMax, rangeMax.capBounds()) }, { &rangeMin, at (rangeMin, rangeMin.capBounds()) },
        { &output, at (output, output.capBounds()) }, { &sensitivity, sensitivity.getBounds().toFloat() },
        { &target, at (target, { target.thumbX (target.getValue()), 0.0f, 29.0f, 22.0f }) },
        { &idleColumn, Rectangle<float> (35.0f, scaleY (proc.idleDb()) - 5.5f, 12.0f, 11.0f) },
        { &responseSwitch, responseSwitch.getBounds().toFloat() }, { &artifacts, artifacts.getBounds().toFloat() } };
    marks.group.clear();
    for (auto& [c, b] : items)
        if (b.intersects (r)) marks.group.push_back (c);
    marks.selected = marks.group.empty() ? nullptr : marks.group.front();
    if (marks.changed) marks.changed();
}

void WBassRideEditor::contentMouseUp (const MouseEvent&)
{
    lassoing = false;
    lasso.setVisible (false);
    if (marks.group.size() < 2) marks.group.clear();
    if (marks.changed) marks.changed();
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
    idleColumn.repaint();
}



//==============================================================================
// the static art, in design coordinates
void WBassRideEditor::paintStatic (Graphics& g) const
{
    // the plug-in's name plate (measured): fine light scratches on the leather, a bass clef (a ball at 20, 529, the
    // stroke thick on its right, a thin tail down to 19, 545, two dots at x 34.5), the name in a bold serif, 12 px
    // capitals from y 526, centred at x 159, silver with a dark edge
    {
        Random rnd (20261025);
        for (int i = 0; i < 260; ++i)
        {
            const float x = rnd.nextFloat() * 320.0f, y = 511.0f + rnd.nextFloat() * 48.0f;
            const float ang = degreesToRadians ((rnd.nextBool() ? 62.0f : 118.0f) + (rnd.nextFloat() - 0.5f) * 30.0f);
            const float len = 3.0f + rnd.nextFloat() * 16.0f;
            g.setColour (Colours::white.withAlpha (0.04f + 0.10f * rnd.nextFloat()));
            g.drawLine (x, y, x + len * std::cos (ang), jlimit (511.0f, 558.0f, y + len * std::sin (ang)), 0.6f);
        }
        Path clef;
        clef.startNewSubPath (17.2f, 527.5f);
        clef.cubicTo (17.0f, 522.5f, 21.5f, 521.0f, 24.8f, 521.4f);
        clef.cubicTo (29.5f, 522.0f, 32.0f, 525.5f, 31.8f, 530.0f);
        clef.cubicTo (31.5f, 536.0f, 26.5f, 541.0f, 19.5f, 545.5f);
        clef.cubicTo (24.5f, 540.5f, 27.6f, 535.5f, 27.6f, 530.0f);
        clef.cubicTo (27.6f, 525.5f, 25.5f, 523.4f, 22.8f, 523.4f);
        clef.cubicTo (20.5f, 523.4f, 19.2f, 525.0f, 19.0f, 527.0f);
        clef.closeSubPath();
        clef.addEllipse (16.6f, 525.6f, 6.8f, 6.8f);
        clef.addEllipse (32.9f, 524.4f, 3.2f, 3.2f);
        clef.addEllipse (32.9f, 531.4f, 3.2f, 3.2f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillPath (clef, AffineTransform::translation (0.6f, 0.8f));
        ColourGradient cg (Colour (238, 238, 238), 0.0f, 521.0f, Colour (200, 200, 200), 0.0f, 546.0f, false);
        g.setGradientFill (cg);
        g.fillPath (clef);

        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions ("Georgia", 20.0f, Font::bold)), "WBass Ride", 0.0f, 0.0f);
        Path text;
        ga.createPath (text);
        const auto tb = text.getBounds();
        const float k = 13.8f / tb.getHeight();                         // the reference's lettering: capitals 526 .. 537
        text.applyTransform (AffineTransform::scale (k).translated (159.0f - tb.getCentreX() * k, 537.6f - tb.getBottom() * k));
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillPath (text, AffineTransform::translation (0.8f, 1.0f));
        ColourGradient silver (Colour (236, 236, 236), 0.0f, 526.0f, Colour (152, 152, 152), 0.0f, 538.0f, false);
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
    g.setColour (kLabelLight.darker (0.08f));
    g.setFont (arial (11.4f, true));
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
        // a screw (measured): a dark ring, a light face, a dark cross
        g.setColour (Colour (24, 24, 20));
        g.drawEllipse (155.2f, y - 4.3f, 8.6f, 8.6f, 1.4f);
        g.setColour (Colour (170, 166, 140));
        g.fillEllipse (156.6f, y - 2.9f, 5.8f, 5.8f);
        g.setColour (Colour (20, 20, 18));
        g.fillRect (157.0f, y - 0.5f, 5.0f, 1.0f);
        g.fillRect (159.0f, y - 2.5f, 1.0f, 5.0f);
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
    // the scales (measured on the reference's 2x skin): a mark every 3 dB (2 px at +-12 and 0, else 1 px), five faint
    // half-pixel ticks between marks at 5, 9, 13, 17 and 21 px below each; numbers every 6 dB centred at x 117.5 and
    // 202 (the Rider) and at +-12 / 0 centred at 291 (the Output)
    g.setFont (arial (10.0f));
    for (int k = -4; k <= 4; ++k)
    {
        const float db = 3.0f * (float) k, y = scaleY (db);
        const bool thick = k == 0 || std::abs (k) == 4, numbered = k % 2 == 0;
        const float h = thick ? 2.0f : 1.0f;
        g.setColour (Colour (19, 19, 16));
        g.fillRect (129.0f, y - h * 0.5f, 19.0f, h);
        g.fillRect (172.0f, y - h * 0.5f, 19.0f, h);
        g.fillRect (270.0f, y - h * 0.5f, thick ? 11.0f : 6.0f, h);
        if (k < 4)
        {
            g.setColour (Colour (101, 98, 80));
            for (int t = 0; t < 5; ++t)
            {
                const float ty = y + 5.0f + 4.0f * (float) t;
                g.fillRect (139.0f, ty - 0.25f, 9.0f, 0.5f);
                g.fillRect (172.0f, ty - 0.25f, 8.0f, 0.5f);
                g.fillRect (270.0f, ty - 0.25f, 4.0f, 0.5f);
            }
        }
        if (numbered)
        {
            const String t = db > 0.0f ? "+" + String ((int) db) : String ((int) db);
            g.setColour (kLabelDark);
            g.drawText (t, Rectangle<float> (97.5f, y - 7.5f, 40.0f, 14.0f), Justification::centred);
            g.drawText (t, Rectangle<float> (182.0f, y - 7.5f, 40.0f, 14.0f), Justification::centred);
            if (thick) g.drawText (t, Rectangle<float> (271.0f, y - 7.5f, 40.0f, 14.0f), Justification::centred);
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
                        grain = 5.0f;
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
    g.setColour (kIdleLine.withAlpha (0.8f));
    g.fillRect (72.0f, scaleY (proc.idleDb()) - 0.5f, 77.0f, 1.0f);
    if (idleColumn.dragging || marks.hover == &idleColumn || marks.showsValue (&idleColumn)) hoverBox (g, { 3.0f, 323.0f, 27.0f, 13.0f }, String (proc.idleDb(), 1));

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
    valueBox (g, { 44.0f, 468.0f, 34.0f, 15.0f }, String (hi - lo, 1));
    valueBox (g, { 143.0f, 468.0f, 34.0f, 15.0f }, String (rider.getValue(), 2));
    valueBox (g, { 241.0f, 468.0f, 34.0f, 15.0f }, String (output.getValue(), 1));

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
