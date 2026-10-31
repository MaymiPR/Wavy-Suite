#include "WBassRiderEditor.h"
#include "WBassRiderSkinData.h"

using namespace juce;
using namespace wbassrider::ui;
using wavy::ren::arial;

namespace
{
// colours measured on the reference's editor (reference/wbassrider, git-ignored)
const Colour kLabelLight (177, 177, 177), kLabelDim (153, 153, 153), kFrame (132, 132, 132), kLabelDark (38, 36, 30),
    kTick (34, 33, 28), kValueBox (0, 0, 0), kValueText (236, 236, 236), kAmberTop (255, 205, 60), kAmberBottom (231, 150, 0),
    kIdleLine (205, 120, 105), kIdleArrow (36, 52, 84), kMarkSelected (241, 227, 7), kMarkMember (7, 150, 229);

// a fitted cap (WBassRiderSkinData.h): grey per row x factor per column, alpha per row x per column
void drawCap (Graphics& g, const wbassrider::skin::Cap& c, float x, float y, int columns = 1 << 20)
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

// a pixel model rendered once into a 1x image (drawn then as one image: drawn pixel by pixel at a window scale that is
// not whole, neighbouring pixels would let what is under them show through)
template <typename Paint>
const Image& pixelImage (Image& slot, int w, int h, Paint paint)
{
    if (! slot.isValid())
    {
        slot = Image (Image::ARGB, w, h, true, SoftwareImageType());   // (a native image does not reach every renderer)
        Graphics gi (slot);
        paint (gi);
    }
    return slot;
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
    // a black box (measured: rows 468.5 .. 482, a lighter bottom edge), 11 px digits
    g.setColour (kValueBox);
    g.fillRoundedRectangle (r, 1.0f);
    g.setColour (Colour (110, 106, 80).withAlpha (0.6f));
    g.fillRect (r.getX() + 1.0f, r.getBottom(), r.getWidth() - 2.0f, 0.6f);
    g.setColour (kValueText);
    g.setFont (arial (11.0f));
    g.drawText (text, r.translated (0.0f, 0.5f), Justification::centred);
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
void wbassrider::ui::drawMark (Graphics& g, Rectangle<float> r, Mark m, bool round)
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

void LinearDrag::drag (Slider& s, const MouseEvent& e, float unitPx, bool vert)
{
    if (! active) return;
    const auto d = e.position - last;
    last = e.position;
    float k = 1.0f;
    if (wavy::ctrlHeldNow()) k = std::abs (vert ? d.y : d.x) <= 2.0f ? 0.037f : 0.19f;
    virt += d * k;
    vertical = vert;
    pxPerUnit = unitPx;
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

// the dome as the reference draws it (WBassRiderSkinData.h: a grey per radius and two harmonics in angle, fitted on its
// frame), centred 0.5 px above the control's centre, rendered once per scale; the pointer (measured): a 2 px dark core
// with soft edges from 2.5 to 17.5 px out, 87 near the centre to 104 at its tip; the selection a yellow ring at the rim
const Image& Knob::dome (float scale)
{
    namespace sk = wbassrider::skin;
    if (domeImage.isValid() && std::abs (scale - domeScale) < 1.0e-3f) return domeImage;
    domeScale = scale;
    const int n = roundToInt (44.0f * scale);
    domeImage = Image (Image::ARGB, n, n, true);
    Image::BitmapData bd (domeImage, Image::BitmapData::writeOnly);
    for (int py = 0; py < n; ++py)
        for (int px = 0; px < n; ++px)
        {
            const float x = ((float) px + 0.5f) / scale - 22.0f, y = ((float) py + 0.5f) / scale - 21.5f;
            const float r = std::sqrt (x * x + y * y);
            const float alpha = jlimit (0.0f, 1.0f, (19.6f - r) / 0.4f);   // the bezel (drawn with the panel) carries the black rim
            if (alpha <= 0.0f) continue;
            const float a = std::atan2 (x, -y);
            const float fi = jlimit (0.0f, (float) (sk::knobDomeRadii - 1) - 1.0e-3f, r / sk::knobDomeStep);
            const int i = (int) fi;
            const float f = fi - (float) i;
            auto at = [a] (int k)
            {
                const float* c = sk::knobDome + k * (1 + 2 * sk::knobDomeHarmonics);
                float v = c[0];
                for (int h = 1; h <= sk::knobDomeHarmonics; ++h) v += c[2 * h - 1] * std::cos ((float) h * a) + c[2 * h] * std::sin ((float) h * a);
                return v;
            };
            const float v = jlimit (0.0f, 255.0f, at (i) * (1.0f - f) + at (i + 1) * f);
            bd.setPixelColour (px, py, Colour::greyLevel (v / 255.0f).withAlpha (alpha));
        }
    return domeImage;
}

void Knob::paint (Graphics& g)
{
    const float s = jlimit (0.25f, 8.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 100.0f) / 100.0f);
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.drawImageAt (dome (s), 0, 0);
    }
    const Point<float> c (22.0f, 21.5f);
    const auto& rp = getRotaryParameters();
    const float a = rp.startAngleRadians + (float) valueToProportionOfLength (getValue()) * (rp.endAngleRadians - rp.startAngleRadians);
    const Point<float> dir (std::sin (a), -std::cos (a));
    ColourGradient pg (Colour (87, 87, 89), c.x + dir.x * 2.5f, c.y + dir.y * 2.5f, Colour (104, 104, 106), c.x + dir.x * 17.5f, c.y + dir.y * 17.5f, false);
    g.setGradientFill (pg);
    g.drawLine (Line<float> (c + dir * 2.5f, c + dir * 17.5f), 2.6f);
    drawMark (g, Rectangle<float> (39.0f, 39.0f).withCentre (c), marks != nullptr ? marks->of (this) : Mark::None, true);
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

// the cap as the reference draws it (WBassRiderSkinData.h, fitted on its 1x frame): outline and lip included
static const wbassrider::skin::FrameCap& frameCap (Fader::Kind k)
{
    namespace sk = wbassrider::skin;
    return k == Fader::Rider ? sk::riderFrame : k == Fader::Output ? sk::outputFrame : k == Fader::RangeMax ? sk::rangeMaxFrame : sk::rangeMinFrame;
}

Rectangle<float> Fader::capBounds() const
{
    const auto& c = frameCap (kind);
    // on whole pixels, as the reference draws its caps (a cap between pixels would let the scale show through it)
    return { (float) (c.x0 - getX()), std::round (yOf (getValue())) + (float) c.dy, (float) c.w, (float) c.h };
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
    namespace sk = wbassrider::skin;
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
        Graphics::ScopedSaveState clip (g);                        // the caps' corners are round
        Path round;
        round.addRoundedRectangle (cap, kind == Rider ? 3.5f : 2.5f);
        g.reduceClipRegion (round);
        static Image images[4];
        g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (images[(int) kind], fc.w, fc.h, [&fc] (Graphics& gi)
        {
            for (int r = 0; r < fc.h; ++r)
                for (int k = 0; k < fc.w; ++k)
                {
                    const float f = fc.cols[k];
                    gi.setColour (Colour ((uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3] * f), (uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3 + 1] * f),
                                          (uint8) jlimit (0.0f, 255.0f, fc.rows[r * 3 + 2] * f)));
                    gi.fillRect (k, r, 1, 1);
                }
        }), (int) cap.getX(), (int) cap.getY());
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
    static Image track, litBar;
    g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (track, 184, 15, [] (Graphics& gi) { drawCap (gi, wbassrider::skin::meterTrack, 0.0f, 0.0f); }), 12, 3);
    if (lit > 0.0f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (12, 3, roundToInt (184.0f * lit), 15);
        g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (litBar, 184, 15, [] (Graphics& gi) { drawCap (gi, wbassrider::skin::meterLit, 0.0f, 0.0f); }), 12, 3);
    }
    // the thumb (fitted on the reference's, WBassRiderSkinData.h): a 1 px dark frame around a white glass, a red line;
    // on whole pixels
    namespace sk = wbassrider::skin;
    const Rectangle<float> t (std::round (thumbX (getValue())), 0.0f, 29.0f, 22.0f);
    static Image thumb;
    g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (thumb, 29, 22, [] (Graphics& gi)
    {
        for (int r = 0; r < 2; ++r)
        {
            gi.setColour (Colours::black.withAlpha (sk::thumbShadow[r]));
            gi.fillRect (3, 19 + r, 23, 1);
        }
        gi.setColour (Colour::greyLevel (sk::thumbFrame / 255.0f));
        gi.drawRect (2, 1, 25, 18, 1);
        for (int r = 0; r < 16; ++r)
            for (int c = 0; c < 23; ++c)
            {
                if (c == 11) continue;                                   // the line's column
                const float al = jlimit (0.0f, 1.0f, sk::thumbGlassAlpha[r] * sk::thumbGlassCol[c]);
                gi.setColour (Colour::greyLevel (sk::thumbGlassGrey[r] / 255.0f).withAlpha (al));
                gi.fillRect (3 + c, 2 + r, 1, 1);
            }
        for (int r = 0; r < 16; ++r)
        {
            const float v = sk::thumbLine[r] / 183.0f;
            gi.setColour (Colour::fromFloatRGBA (jmin (1.0f, 0.95f * v + 0.1f), 0.12f * v, 0.12f * v, 1.0f));
            gi.fillRect (14, 2 + r, 1, 1);
        }
    }), (int) t.getX(), 0);
    drawMark (g, t, marks != nullptr ? marks->of (this) : Mark::None);
}

//==============================================================================
// the switch (37 x 17): a dark slot, the ridged chrome block on the left (Slow) or the right (Fast)
void Switch::paintButton (Graphics& g, bool, bool)
{
    // as the reference draws it (WBassRiderSkinData.h, fitted on its frame): a dark 1 px frame, the slot graded by the
    // distance from the block, the ridged block (a rank-2 model) on the left (Slow) or 15 px right (Fast); selected,
    // the frame turns yellow
    namespace sk = wbassrider::skin;
    const bool fast = getToggleState();
    static Image states[2];
    g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (states[fast ? 1 : 0], 37, 17, [fast] (Graphics& gi)
    {
        gi.setColour (Colour::greyLevel (sk::switchFrame / 255.0f));
        gi.fillRect (0, 0, 37, 17);
        for (int k = 0; k < 15; ++k)
        {
            const int x = fast ? 15 - k : 21 + k;                          // the slot: k from the block outwards
            gi.setColour (Colour::greyLevel (sk::switchSlot[k] / 255.0f));
            gi.fillRect (x, 1, 1, 15);
        }
        const int bx = fast ? 16 : 1;
        for (int r = 0; r < 15; ++r)
            for (int c = 0; c < 20; ++c)
            {
                const float v = sk::switchBlockU[r * 2] * sk::switchBlockV[c * 2] + sk::switchBlockU[r * 2 + 1] * sk::switchBlockV[c * 2 + 1];
                gi.setColour (Colour::greyLevel (jlimit (0.0f, 255.0f, v) / 255.0f));
                gi.fillRect (bx + c, 1 + r, 1, 1);
            }
    }), 0, 0);
    if (marks != nullptr && marks->of (this) != Mark::None)
    {
        g.setColour (kMarkSelected);
        g.drawRect (getLocalBounds(), 1);
    }
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
// the cursor over the knobs and Output (the reference's DisplayCursor controls, filmed with MeterCapture): an arrow
// with a small up / down arrow at its lower right, the hotspot at the arrow's tip (6, 7)
static MouseCursor dragCursor()
{
    Image img (Image::ARGB, 32, 32, true);
    {
        Graphics g (img);
        Path arrow;
        arrow.startNewSubPath (6.5f, 7.5f);
        arrow.lineTo (6.5f, 21.5f); arrow.lineTo (9.5f, 18.5f); arrow.lineTo (12.0f, 24.0f); arrow.lineTo (14.0f, 23.0f);
        arrow.lineTo (11.5f, 17.5f); arrow.lineTo (15.5f, 17.5f); arrow.closeSubPath();
        g.setColour (Colours::white); g.fillPath (arrow);
        g.setColour (Colours::black); g.strokePath (arrow, PathStrokeType (1.0f));
        Path updown;
        updown.startNewSubPath (22.5f, 17.5f); updown.lineTo (25.5f, 20.5f); updown.lineTo (23.5f, 20.5f); updown.lineTo (23.5f, 23.5f);
        updown.lineTo (25.5f, 23.5f); updown.lineTo (22.5f, 26.5f); updown.lineTo (19.5f, 23.5f); updown.lineTo (21.5f, 23.5f);
        updown.lineTo (21.5f, 20.5f); updown.lineTo (19.5f, 20.5f); updown.closeSubPath();
        g.setColour (Colours::white); g.fillPath (updown);
        g.setColour (Colours::black); g.strokePath (updown, PathStrokeType (1.0f));
    }
    return MouseCursor (img, 6, 7);
}

//==============================================================================
WBassRiderEditor::WBassRiderEditor (WBassRiderProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wbassrider::mode), "WBass Rider", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Renaissance, wbassrider::modeNames()),
      lasso (make<wavy::flat::LassoOverlay>()),
      proc (p),
      sensitivity (make<Knob>()), spill (make<Knob>()), artifacts (make<Knob>()), responseSwitch (make<Switch>()),
      target (make<TargetSlider>()), rangeMax (make<Fader> (Fader::RangeMax)), rangeMin (make<Fader> (Fader::RangeMin)),
      output (make<Fader> (Fader::Output)), rider (make<Fader> (Fader::Rider)), idleColumn (make<IdleColumn>())
{
    setFactoryPresets (wbassrider::factoryPresets());             // Target stays where it is, as in the reference, unless a preset sets it
    marks.blueAlone = { &rider, &idleColumn };
    marks.changed = [this] { content().repaint(); };
    const auto dragging = dragCursor();
    for (Component* c : { (Component*) &sensitivity, (Component*) &spill, (Component*) &artifacts, (Component*) &output })
        c->setMouseCursor (dragging);
    marks.enter = [this] (Slider* sl) { openEntry (sl); };
    for (auto* k : { &sensitivity, &spill, &artifacts }) k->marks = &marks;
    for (auto* f : { &rangeMax, &rangeMin, &output, &rider }) f->marks = &marks;
    target.marks = &marks;
    responseSwitch.marks = &marks;

    // control rectangles from the reference's GUIXML
    attachSlider (sensitivity, wbassrider::sensitivity, { 24, 90, 44, 44 }, 50.0);
    attachSlider (spill, wbassrider::spill, { 178, 90, 44, 44 }, -40.0);
    attachSlider (artifacts, wbassrider::artifacts, { 253, 90, 44, 44 }, 50.0);
    attach (responseSwitch, wbassrider::response, { 104, 106, 37, 17 });
    attachSlider (target, wbassrider::target, { 59, 45, 210, 22 }, -14.0);
    attachSlider (rangeMax, wbassrider::rangeMax, { 36, 190, 48, 270 }, 12.0);
    attachSlider (rangeMin, wbassrider::rangeMin, { 36, 190, 48, 270 }, -12.0);
    attachSlider (output, wbassrider::output, { 235, 190, 48, 270 }, 0.0);
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
    // the knobs' and the Target's value boxes are painted by the panel, so they follow a drag only if it repaints them
    for (auto* k : { &sensitivity, &spill, &artifacts })
        k->onValueChange = [this, k] { content().repaint (k->getX(), 150, k->getWidth() + 20, 28); };
    target.onValueChange = [this] { content().repaint (266, 42, 44, 26); };
    // the Idle arrow
    auto* idleParam = p.apvts.getParameter (wbassrider::idle);
    idleColumn.marks = &marks;
    idleColumn.idleDb = [this] { return proc.idleDb(); };
    idleColumn.setIdleDb = [this, idleParam] (float db)
    {
        const float lo = proc.get (wbassrider::rangeMin), hi = proc.get (wbassrider::rangeMax);
        idleParam->setValueNotifyingHost (idleParam->convertTo0to1 (jlimit (0.02f, 0.98f, (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo))));
        content().repaint();
    };
    idleColumn.gesture = [idleParam] (bool begin) { if (begin) idleParam->beginChangeGesture(); else idleParam->endChangeGesture(); };
    idleColumn.reset = [this, idleParam] { idleParam->setValueNotifyingHost (idleParam->getDefaultValue()); content().repaint(); };
    addToContent (idleColumn, { 33, 214, 16, 226 });
    addToContent (lasso, { 0, 0, W, H });                          // over everything, takes no clicks
    lasso.setVisible (false);
}

WBassRiderEditor::~WBassRiderEditor()
{
    entry.reset();
    for (auto* k : { &sensitivity, &spill, &artifacts }) k->marks = nullptr;
}

Rectangle<int> WBassRiderEditor::entryArea (Slider* sl) const
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

void WBassRiderEditor::openEntry (Slider* sl)
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

void WBassRiderEditor::closeEntry (bool apply)
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
    MessageManager::callAsync ([owned = std::shared_ptr<TextEditor> (std::move (e))] {});   // destroyed after its callback returns
    content().repaint();
}

// the lasso (measured on the reference: a 1 px white rectangle while dragging on the background; the controls it
// touches, in the reference's control order: Spill, Rider, Range Max, Range Min, Output, Sensitivity, Target, Idle,
// Response, Artifacts)
void WBassRiderEditor::contentMouseDown (const MouseEvent& e)
{
    if (Rectangle<float> (279.0f, 183.0f, 17.0f, 15.0f).contains (e.position))   // the clip LED: a click clears it
    {
        clip = false;
        content().repaint (276, 182, 24, 18);
        return;
    }
    lassoing = true;
    lassoStart = e.position;
    marks.group.clear();
    marks.selected = nullptr;
    if (marks.changed) marks.changed();
}

void WBassRiderEditor::contentMouseDrag (const MouseEvent& e)
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

void WBassRiderEditor::contentMouseUp (const MouseEvent&)
{
    lassoing = false;
    lasso.setVisible (false);
    if (marks.group.size() < 2) marks.group.clear();
    if (marks.changed) marks.changed();
}

void WBassRiderEditor::tick()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001, dt = lastTick > 0.0 ? now - lastTick : 0.0;
    lastTick = now;
    // the Energy meter's display (measured on the reference's films): it rises at once and falls 37.3 dB per second at
    // most (slower when the energy itself falls slower)
    const float e = proc.energyDb.take();
    energy = e >= energy ? e : jmax (e, energy - 37.3f * (float) jmin (dt, 0.25));
    target.energyDb = energy;
    target.repaint();
    if (proc.outDb.take() >= 0.0f && ! clip) { clip = true; content().repaint (276, 182, 24, 18); }   // latched
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
void WBassRiderEditor::paintStatic (Graphics& g) const
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
        ga.addLineOfText (Font (FontOptions ("Georgia", 20.0f, Font::bold)), "WBass Rider", 0.0f, 0.0f);
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

    // ---- top panel: the group frames (2 px of grey 132 at 0.69, measured on x 7-8 / 311-312, y 75-77 and 154-155 over
    // three panel greys), stroked as one path so the shared stems are not drawn twice
    const PathStrokeType frameStroke (2.0f);
    // each group (traced on the skin's 2x background): its title gap in the top line, the outer corners quadratic
    // fillets 10 px each way; at the junction (x 162) the top and bottom lines bend down / up in the same 10 px fillets
    // into short stems ending at y 87 and 144.6, the side open
    Path frames;
    auto group = [&frames] (float xOuter, float gapOuter, float gapJoin)    // the title gap's ends
    {
        const float top = 76.5f, bottom = 155.0f, r = 10.0f, xJoin = 162.0f, dir = xJoin > xOuter ? 1.0f : -1.0f;
        frames.startNewSubPath (gapOuter, top);
        frames.lineTo (xOuter + dir * r, top);
        frames.quadraticTo (xOuter, top, xOuter, top + r);
        frames.lineTo (xOuter, bottom - r);
        frames.quadraticTo (xOuter, bottom, xOuter + dir * r, bottom);
        frames.lineTo (xJoin - dir * r, bottom);
        frames.quadraticTo (xJoin, bottom, xJoin, bottom - r);
        frames.lineTo (xJoin, 144.6f);
        frames.startNewSubPath (gapJoin, top);
        frames.lineTo (xJoin - dir * r, top);
        frames.quadraticTo (xJoin, top, xJoin, top + r);
        frames.lineTo (xJoin, 87.0f);
    };
    group (8.0f, 61.0f, 101.5f);
    group (312.0f, 258.5f, 218.0f);
    {
        // opaque into a layer at the physical scale, then laid on at 0.69 (a transparency layer here comes out at 1x)
        const float ps = g.getInternalContext().getPhysicalPixelScaleFactor();
        Image layer (Image::ARGB, roundToInt ((float) W * ps), roundToInt (170.0f * ps), true);
        {
            Graphics gl (layer);
            gl.addTransform (AffineTransform::scale (ps));
            gl.setColour (kFrame);
            gl.strokePath (frames, frameStroke);
        }
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / ps));
        g.setOpacity (0.69f);
        g.drawImageAt (layer, 0, 0);
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
    g.drawText ("Slow", Rectangle<float> (76.0f, 108.0f, 24.0f, 12.0f), Justification::centred);
    g.drawText ("Fast", Rectangle<float> (141.5f, 108.0f, 24.0f, 12.0f), Justification::centred);

    // the knobs' scales, measured on the reference's frame round the domes' centres (x 46 / 200 / 275, y 111.5):
    // Sensitivity (measured on the skin's 2x background, where the dome does not hide them): 12 dashes on a ring of its
    // own, centred 0.4 px left of and 1.4 px below the knob's (so they seem to drift out towards the ends), radius 21.1
    // to 25.25, sectors 16.5 degrees long at the measured angles (about +-(10.7 + 22.1 k)), grey 131 (126 at the outermost
    // pair);
    // Spill / Artifacts: 13 ticks of their own lengths and greys (angle, inner and outer radius, grey)
    {
        const Rectangle<float> ring = Rectangle<float> (50.5f, 50.5f).withCentre ({ 45.6f, 112.9f });
        static constexpr float dashes[] = { -122.3f, -100.0f, -76.7f, -54.0f, -32.0f, -10.0f, 11.4f, 33.2f, 54.6f, 76.6f, 98.8f, 120.2f };
        for (float mid : dashes)
        {
            g.setColour (Colour::greyLevel ((std::abs (mid) > 110.0f ? 126.0f : 131.0f) / 255.0f));
            Path seg;
            seg.addPieSegment (ring, degreesToRadians (mid - 8.25f), degreesToRadians (mid + 8.25f), 21.1f / 25.25f);
            g.fillPath (seg);
        }
        struct Tick { float angle, r0, r1, grey; };
        static constexpr Tick ticks[] = { { 0.0f, 20.5f, 25.8f, 147.0f }, { 23.0f, 20.3f, 24.0f, 140.0f }, { 46.0f, 20.3f, 25.6f, 140.0f },
                                          { 69.0f, 20.5f, 24.3f, 135.0f }, { 91.0f, 21.0f, 26.8f, 147.0f }, { 115.0f, 21.3f, 25.3f, 115.0f },
                                          { 135.0f, 21.3f, 27.3f, 100.0f } };
        for (float cx : { 200.0f, 275.0f })
            for (auto& t : ticks)
                for (float side : { -1.0f, 1.0f })
                {
                    if (juce::exactlyEqual (t.angle, 0.0f) && side > 0.0f) continue;
                    const float a = degreesToRadians (side * t.angle);
                    const Point<float> c (cx, 111.5f), d (std::sin (a), -std::cos (a));
                    g.setColour (Colour::greyLevel (t.grey / 255.0f));
                    g.drawLine (Line<float> (c + d * t.r0, c + d * t.r1), 2.0f);
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
        // a screw (measured): centred at 159.5, a dark ring of radius 4 on a light face, a faint lit ring outside it
        // (brightest below), a small dark plus at the centre
        const Point<float> c (159.5f, y);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (10.6f, 10.6f).withCentre (c.translated (0.0f, 0.4f)), 1.0f);
        g.setColour (Colour (150, 147, 125));
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
        g.setColour (Colour (10, 10, 8));
        g.drawEllipse (Rectangle<float> (8.4f, 8.4f).withCentre (c), 1.3f);
        g.fillRect (c.x - 1.5f, c.y - 0.5f, 3.0f, 1.0f);
        g.fillRect (c.x - 0.5f, c.y - 1.5f, 1.0f, 3.0f);
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

// the knobs' cast shadow (measured on the reference's frame against its panel; the light comes from straight above): the
// knob's disc (radius 21.8) swept 22.5 px down, its corners rounding off at the panel's foot, with a 1.6 px edge: 0.80 of
// the panel just under the knob, lifting to 0.92 at the panel's foot; and a darker band right under the lower half of the
// rim (0.32 deep, fading over 2.3 px)
static float knobShadowAt (float x, float y)
{
    float f = 1.0f;
    for (float cx : { 46.0f, 200.0f, 275.0f })
    {
        const float dx = x - cx, dy = y - 111.5f, r = std::sqrt (dx * dx + dy * dy);
        if (r < 20.0f || std::abs (dx) > 24.0f) continue;
        const float rim = 0.32f * std::exp (-(r - 20.5f) / 2.3f) * jlimit (0.0f, 1.0f, dy / 20.0f);
        const float sy = dy - jlimit (0.0f, 22.5f, dy), d = std::sqrt (dx * dx + sy * sy);
        const float cast = 0.21f * std::exp (-jmax (0.0f, dy - 22.0f) / 22.0f) * jlimit (0.0f, 1.0f, (21.8f - d) / 1.6f + 0.5f);
        f *= 1.0f - jlimit (0.0f, 0.9f, rim + cast);
    }
    return f;
}

// the bezel round each knob (WBassRiderSkinData.h, fitted on the reference's frame between the ticks and dashes): the
// black rim, then round Spill and Artifacts a light metal ring lit from above (about +75 over the panel at the sides,
// lost in the shadow at the foot), as an offset over the shadowed panel that dies out by radius 24
// the top panel's correction over rows times columns (WBassRiderSkinData.h: the frame over the model on 8 px cells,
// linear between their centres)
static float topPanelFixAt (float x, float y)
{
    namespace sk = wbassrider::skin;
    const float fx = jlimit (0.0f, (float) sk::topPanelFixW - 1.001f, (x - sk::topPanelFixX0) / sk::topPanelFixStep);
    const float fy = jlimit (0.0f, (float) sk::topPanelFixH - 1.001f, (y - sk::topPanelFixY0) / sk::topPanelFixStep);
    const int i = (int) fx, j = (int) fy;
    const float tx = fx - (float) i, ty = fy - (float) j;
    auto at = [] (int jj, int ii) { return sk::topPanelFix[jj * sk::topPanelFixW + ii]; };
    return (at (j, i) * (1.0f - tx) + at (j, i + 1) * tx) * (1.0f - ty) + (at (j + 1, i) * (1.0f - tx) + at (j + 1, i + 1) * tx) * ty;
}

static void knobBezelAt (float x, float y, float rgb[3])
{
    namespace sk = wbassrider::skin;
    for (float cx : { 46.0f, 200.0f, 275.0f })
    {
        const float dx = x - cx, dy = y - 111.5f, r = std::sqrt (dx * dx + dy * dy);
        if (r >= 24.0f) continue;
        const float a = std::atan2 (dx, -dy);
        const float fi = jlimit (0.0f, (float) (sk::knobBezelRadii - 1) - 1.0e-3f, (r - sk::knobBezelR0) / sk::knobBezelStep);
        const int i = (int) fi;
        const float t = fi - (float) i;
        const bool sens = cx < 100.0f;                                  // Sensitivity: the black rim alone
        const float* table = sens ? sk::knobBezelSens : sk::knobBezel;
        const int nh = sens ? sk::knobBezelSensHarmonics : sk::knobBezelHarmonics;
        auto at = [a, table, nh] (int k)
        {
            const float* c = table + k * (1 + 2 * nh);
            float v = c[0];
            for (int h = 1; h <= nh; ++h) v += c[2 * h - 1] * std::cos ((float) h * a) + c[2 * h] * std::sin ((float) h * a);
            return v;
        };
        const float v = at (i) * (1.0f - t) + at (i + 1) * t;
        for (int k = 0; k < 3; ++k) rgb[k] = jlimit (0.0f, 255.0f, rgb[k] + v);
        return;
    }
}

void WBassRiderEditor::paintContent (Graphics& g)
{
    namespace sk = wbassrider::skin;
    // the static art, rendered once per physical scale (the window's size times the display's: the panels from their
    // fitted models, pixel by pixel, then the art)
    const float s = jlimit (0.25f, 8.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 100.0f) / 100.0f);
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
                        if (x >= 6.5f - (y - 36.0f) / 126.0f && x < 314.0f)
                        {
                            panelAt (sk::topPanel, x, y, rgb); grain = 3.2f;
                            const float f = knobShadowAt (x, y) * topPanelFixAt (x, y);
                            for (auto& v : rgb) v *= f;
                            knobBezelAt (x, y, rgb);
                        }
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
    const float lo = proc.get (wbassrider::rangeMin), hi = proc.get (wbassrider::rangeMax);
    g.setColour (Colour (255, 253, 209).withAlpha (0.21f));      // a warm light (measured on the band against plain cream)
    g.fillRect (Rectangle<float>::leftTopRightBottom (72.0f, scaleY (hi), 149.0f, scaleY (lo)));
    g.setColour (kIdleLine.withAlpha (0.8f));
    g.fillRect (72.0f, scaleY (proc.idleDb()) - 0.5f, 77.0f, 1.0f);
    if (idleColumn.dragging || marks.hover == &idleColumn || marks.showsValue (&idleColumn)) hoverBox (g, { 3.0f, 323.0f, 27.0f, 13.0f }, String (proc.idleDb(), 1));

    // the clip LED (measured on the frames): a dark frame x 280 .. 294, y 186 .. 195, the inside by row, off or lit; lit,
    // a red glow beside and below it. It stays lit until it is clicked (as the reference's)
    {
        namespace sk = wbassrider::skin;
        static Image states[2];
        const bool on = clip;
        g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (pixelImage (states[on ? 1 : 0], 17, 11, [on] (Graphics& gi)
        {
            gi.setColour (Colour::greyLevel (sk::ledFrame / 255.0f));
            gi.fillRect (1, 0, 15, 10);
            const float* rows = on ? sk::ledOnRows : sk::ledOffRows;
            for (int r = 0; r < 8; ++r)
            {
                const Colour c ((uint8) rows[r * 3], (uint8) rows[r * 3 + 1], (uint8) rows[r * 3 + 2]);
                gi.setColour (c.darker (0.06f));
                gi.fillRect (2, 1 + r, 13, 1);
                gi.setColour (c);
                gi.fillRect (3, 1 + r, 11, 1);
            }
            if (on)
            {
                for (int r = 0; r < 6; ++r)
                {
                    gi.setColour (Colour ((uint8) sk::ledGlowSide[r * 3], (uint8) sk::ledGlowSide[r * 3 + 1], (uint8) sk::ledGlowSide[r * 3 + 2]));
                    gi.fillRect (0, 4 + r, 1, 1);
                    gi.fillRect (16, 4 + r, 1, 1);
                }
                gi.setColour (Colour ((uint8) sk::ledGlowBelow[0], (uint8) sk::ledGlowBelow[1], (uint8) sk::ledGlowBelow[2]));
                gi.fillRect (2, 10, 13, 1);
            }
        }), 279, 186);
    }

    // value boxes
    valueBox (g, { 44.0f, 468.5f, 34.0f, 13.5f }, String (hi - lo, 1));
    valueBox (g, { 143.0f, 468.5f, 34.0f, 13.5f }, String (rider.getValue(), 2));
    valueBox (g, { 241.0f, 468.5f, 34.0f, 13.5f }, String (output.getValue(), 1));

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
    if (wbassrider::isLive (proc.modeIndex()))
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
