#include "WVoxRiderEditor.h"
#include "WVoxRiderSkinData.h"

using namespace juce;
using namespace wvoxrider::ui;
using wavy::ren::arial;
namespace sk = wvoxrider::skin;

namespace
{
// colours measured on the reference's editor (reference/wvoxrider, git-ignored)
const Colour kValueBox (0, 0, 0), kValueText (255, 255, 255), kIdleLine (228, 125, 120), kMarkSelected (241, 227, 7),
    kMarkMember (7, 150, 229), kScaleInk (51, 51, 51);

float physicalScale (Graphics& g)
{
    return jlimit (0.25f, 8.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 100.0f) / 100.0f);
}

// premultiplied channels 0 .. 1 into a pixel
void setPremultiplied (Image::BitmapData& bd, int x, int y, float r, float gr, float b, float a)
{
    a = jlimit (0.0f, 1.0f, a);
    if (a <= 1.0e-4f) return;
    bd.setPixelColour (x, y, Colour::fromFloatRGBA (jlimit (0.0f, 1.0f, r / a), jlimit (0.0f, 1.0f, gr / a), jlimit (0.0f, 1.0f, b / a), a));
}

// a low-rank part rendered at a physical scale (its rows and columns interpolated linearly)
Image renderLowRank (const sk::LowRank& m, float s)
{
    const int w = jmax (1, roundToInt ((float) m.w * s)), h = jmax (1, roundToInt ((float) m.h * s)), k = m.k;
    Image img (Image::ARGB, w, h, true, SoftwareImageType());
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    auto sample = [k] (const float* t, int n, float f, float* out)           // linear between pixel centres, clamped
    {
        f = jlimit (0.0f, (float) (n - 1), f);
        const int i = jmin ((int) f, n - 2 < 0 ? 0 : n - 2);
        const float u = n > 1 ? f - (float) i : 0.0f;
        for (int j = 0; j < k * 4; ++j) out[j] = t[i * k * 4 + j] * (1.0f - u) + (n > 1 ? t[(i + 1) * k * 4 + j] * u : 0.0f);
    };
    std::vector<float> cols ((size_t) w * (size_t) k * 4), row ((size_t) k * 4);
    for (int px = 0; px < w; ++px) sample (m.v, m.w, ((float) px + 0.5f) / s - 0.5f, cols.data() + (size_t) px * (size_t) k * 4);
    for (int py = 0; py < h; ++py)
    {
        sample (m.u, m.h, ((float) py + 0.5f) / s - 0.5f, row.data());
        for (int px = 0; px < w; ++px)
        {
            const float* c = cols.data() + (size_t) px * (size_t) k * 4;
            float v[4] = { 0, 0, 0, 0 };
            for (int j = 0; j < k; ++j)
                for (int ch = 0; ch < 4; ++ch) v[ch] += row[(size_t) (j * 4 + ch)] * c[j * 4 + ch];
            setPremultiplied (bd, px, py, v[0], v[1], v[2], v[3]);
        }
    }
    return img;
}

// a polar part rendered at a physical scale into a w x h (design px) image; its pointer (if any) turned to angle
// (degrees from straight up, clockwise)
Image renderPolar (const sk::Polar& m, int w, int h, float s, float angle, float rmax)
{
    const int iw = jmax (1, roundToInt ((float) w * s)), ih = jmax (1, roundToInt ((float) h * s));
    Image img (Image::ARGB, iw, ih, true, SoftwareImageType());
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    const int nb = 1 + 2 * m.nh;
    const float pa = degreesToRadians (angle), ux = std::sin (pa), uy = -std::cos (pa);
    std::vector<float> basis ((size_t) nb);
    for (int py = 0; py < ih; ++py)
        for (int px = 0; px < iw; ++px)
        {
            const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
            const float dx = x - m.cx, dy = y - m.cy, r = std::sqrt (dx * dx + dy * dy);
            if (r > rmax) continue;
            const float th = std::atan2 (dx, -dy);
            basis[0] = 1.0f;
            for (int k = 1; k <= m.nh; ++k) { basis[(size_t) (2 * k - 1)] = std::cos ((float) k * th); basis[(size_t) (2 * k)] = std::sin ((float) k * th); }
            const float fi = jlimit (0.0f, (float) (m.radii - 1) - 1.0e-3f, r / m.step);
            const int i = (int) fi;
            const float t = fi - (float) i;
            float v[4];
            for (int ch = 0; ch < 4; ++ch)
            {
                float a = 0.0f, b = 0.0f;
                for (int j = 0; j < nb; ++j)
                {
                    a += basis[(size_t) j] * m.c[(i * nb + j) * 4 + ch];
                    b += basis[(size_t) j] * m.c[((i + 1) * nb + j) * 4 + ch];
                }
                v[ch] = a * (1.0f - t) + b * t;
            }
            if (m.pointer != nullptr)
            {
                // the pointer's difference layer, along it from the centre and across it to its right
                const float al = dx * ux + dy * uy, ac = -dx * uy + dy * ux;
                const float fa = al - m.along0, fc = ac - m.across0;   // (1 px steps)
                if (fa >= 0.0f && fa <= (float) (m.along - 1) && fc >= 0.0f && fc <= (float) (m.across - 1))
                {
                    const int ia = jmin ((int) fa, m.along - 2), ic = jmin ((int) fc, m.across - 2);
                    const float ta = fa - (float) ia, tc = fc - (float) ic;
                    for (int ch = 0; ch < 4; ++ch)
                    {
                        auto d = [&] (int a, int c) { return m.pointer[(a * m.across + c) * 4 + ch]; };
                        v[ch] += (d (ia, ic) * (1.0f - tc) + d (ia, ic + 1) * tc) * (1.0f - ta) + (d (ia + 1, ic) * (1.0f - tc) + d (ia + 1, ic + 1) * tc) * ta;
                    }
                }
            }
            setPremultiplied (bd, px, py, v[0], v[1], v[2], v[3]);
        }
    return img;
}

// a sprite for the current physical scale, made on demand
template <typename Make>
const Image& sprite (Sprite& sp, float s, Make make)
{
    if (! sp.image.isValid() || std::abs (sp.scale - s) > 1.0e-3f) { sp.image = make (s); sp.scale = s; }
    return sp.image;
}

// draws a sprite rendered at physical scale s with its top left at (x, y) design px
void drawSprite (Graphics& g, const Image& img, float s, float x, float y)
{
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::scale (1.0f / s));
    g.setOpacity (1.0f);                                           // (drawImageAt takes the colour's alpha)
    g.drawImageAt (img, roundToInt (x * s), roundToInt (y * s));
}

// a printed label, its glyphs fitted into the ink box measured on the reference (the box holds the pixels that stand
// out by 30 levels, so the outline reaches a fifth of a pixel past it)
void drawLabel (Graphics& g, const sk::Label& l)
{
    GlyphArrangement ga;
    ga.addLineOfText (arial (12.0f, l.bold), String (l.text), 0.0f, 0.0f);
    Path p;
    ga.createPath (p);
    const auto b = p.getBounds();
    if (b.isEmpty()) return;
    const auto box = Rectangle<float>::leftTopRightBottom (l.x0, l.y0, l.x1, l.y1).expanded (0.2f);
    p.applyTransform (AffineTransform::fromTargetPoints (b.getX(), b.getY(), box.getX(), box.getY(),
                                                          b.getRight(), b.getY(), box.getRight(), box.getY(),
                                                          b.getX(), b.getBottom(), box.getX(), box.getBottom()));
    g.setColour (Colour::greyLevel (l.grey / 255.0f));
    g.fillPath (p);
}

void drawRuns (Graphics& g, const sk::Run* runs, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        g.setColour (Colour::greyLevel (runs[i].grey / 255.0f).withAlpha (runs[i].alpha));
        g.fillRect (runs[i].x, runs[i].y, runs[i].w, runs[i].h);
    }
}

// a knob's printed scale (fitted on the reference's background: tools/fit_voxrider_parts.py scale_strokes)
void drawScale (Graphics& g, const sk::Scale& sc, Point<float> c)
{
    const float end = degreesToRadians (sc.end);
    const Point<float> rc (c.x, c.y + sc.dy);
    auto onRing = [&] (float a) { return rc + Point<float> (std::sin (a), -std::cos (a)) * sc.r; };
    Path ring;
    ring.addCentredArc (rc.x, rc.y, sc.r, sc.r, 0.0f, -end, end, true);
    g.setColour (Colour::greyLevel (sc.grey / 255.0f));
    g.strokePath (ring, PathStrokeType (sc.w, PathStrokeType::mitered, PathStrokeType::butt));
    // ticks: along rays from (0, tickDy), from where they meet the ring
    for (int k = -sc.n; k <= sc.n; ++k)
    {
        const float a = degreesToRadians (k == 0 ? 0.0f : std::copysign (sc.angles[std::abs (k)], (float) k));
        const float len = sc.lengths[std::abs (k)];
        if (len <= 0.05f) continue;
        const Point<float> u (std::sin (a), -std::cos (a));
        const float oy = sc.tickDy - sc.dy, bq = oy * u.y, cq = oy * oy - sc.r * sc.r;
        const float s = -bq + std::sqrt (jmax (0.0f, bq * bq - cq));
        const Point<float> hit (c.x + s * u.x, c.y + sc.tickDy + s * u.y);
        g.drawLine (Line<float> (hit, hit + u * len), sc.tickW);
    }
    // the ends: the ring bent in, then a flare out
    for (float side : { -1.0f, 1.0f })
    {
        const Point<float> e = onRing (side * end);
        const float b2 = side * degreesToRadians (sc.end + sc.bendAngle), b3 = side * degreesToRadians (sc.end + sc.flareAngle);
        const Point<float> v = e + Point<float> (std::sin (b2), -std::cos (b2)) * sc.bendLength;
        g.setColour (Colour::greyLevel (sc.grey / 255.0f));
        g.drawLine (Line<float> (e, v), sc.w);
        g.setColour (Colour::greyLevel (sc.flareGrey / 255.0f));
        g.drawLine (Line<float> (v, v + Point<float> (std::sin (b3), -std::cos (b3)) * sc.flareLength), sc.flareW);
    }
}

// a value in a reference-style black box (Range, Rider, Output; measured: white 7 px digits on the baseline 487,
// centred half a pixel left of the box's centre)
void valueBox (Graphics& g, Rectangle<float> r, const String& text)
{
    GlyphArrangement ga;
    ga.addLineOfText (arial (10.0f), text, 0.0f, 487.0f);
    const auto b = ga.getBoundingBox (0, -1, true);
    g.setColour (kValueText);
    ga.draw (g, AffineTransform::translation (r.getCentreX() - 0.5f - b.getCentreX(), 0.0f));
}

// a selected control's value (as WBass Rider's): a dark box in a 1 px white frame, light grey text
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

// the layouts (the reference's GUIXML control rectangles: Studio, Live)
struct Layout { Rectangle<int> vocal, music, spill, attack, leds, musicLed, vocalValue, musicValue, spillValue; };
const Layout kStudio { { 83, 88, 54, 60 }, { 183, 88, 54, 60 }, {}, { 21, 124, 13, 27 }, { 19, 68, 17, 36 }, { 254, 67, 33, 40 },
                       { 91, 165, 37, 13 }, { 192, 165, 37, 13 }, {} };
const Layout kLive { { 133, 88, 54, 60 }, { 230, 88, 54, 60 }, { 71, 111, 36, 36 }, { 16, 136, 13, 27 }, { 14, 69, 17, 36 }, { 281, 65, 33, 40 },
                     { 159, 165, 37, 13 }, { 242, 165, 37, 13 }, { 70, 165, 37, 13 } };
}

//==============================================================================
void wvoxrider::ui::drawMark (Graphics& g, Rectangle<float> r, Mark m, bool round)
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
    e.source.enableUnboundedMouseMovement (true, false);
    s.Slider::mouseDown (e);
    return true;
}

void LinearDrag::drag (Slider& s, const MouseEvent& e, float ppu, bool vert)
{
    if (! active) return;
    const auto d = e.position - last;
    last = e.position;
    float k = 1.0f;
    if (wavy::ctrlHeldNow()) k = std::abs (vert ? d.y : d.x) <= 2.0f ? 0.037f : 0.19f;
    virt += d * k;
    vertical = vert;
    pxPerUnit = ppu;
    const float moved = vert ? origin.y - virt.y : virt.x - origin.x;
    s.setValue (jlimit (s.getMinimum(), s.getMaximum(), start + (double) (moved / ppu)), sendNotificationSync);
}

void LinearDrag::up (Slider& s, const MouseEvent& e)
{
    if (! active) return;
    active = false;
    s.Slider::mouseUp (e);
    e.source.enableUnboundedMouseMovement (false);
    const float moved = (float) (s.getValue() - start) * pxPerUnit;
    const auto at = vertical ? juce::Point<float> (origin.x, origin.y - moved) : juce::Point<float> (origin.x + moved, origin.y);
    juce::Desktop::setMousePosition (s.localPointToGlobal (at).roundToInt());
}

//==============================================================================
Knob::Knob (bool isSmall) : small (isSmall)
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setRotaryParameters (degreesToRadians (360.0f + sk::knobAngle0), degreesToRadians (360.0f + sk::knobAngle1), true);
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

// the reference's strip has 71 frames: the value picks the nearest; each frame is the dome's model with the pointer's
// layer turned to the frame's angle
void Knob::paint (Graphics& g)
{
    const float s = physicalScale (g);
    const auto& m = small ? sk::spillKnob : sk::bigKnob;
    const int w = small ? 36 : 54, h = small ? 36 : 60;
    if (disabledLook && ! small)
    {
        drawSprite (g, sprite (disabled, s, [] (float sc) { return renderPolar (sk::bigKnobDisabled, 54, 60, sc, sk::knobDisabledAngle, 27.5f); }), s, 0.0f, 0.0f);
    }
    else
    {
        const int f = jlimit (0, 70, roundToInt (valueToProportionOfLength (getValue()) * 70.0));
        const float angle = sk::knobAngle0 + (sk::knobAngle1 - sk::knobAngle0) * (float) f / 70.0f;
        drawSprite (g, sprite (frames[f], s, [&] (float sc) { return renderPolar (m, w, h, sc, angle, small ? 17.5f : 26.5f); }), s, 0.0f, 0.0f);
    }
    drawMark (g, Rectangle<float> (small ? 31.0f : 44.0f, small ? 31.0f : 44.0f).withCentre ({ m.cx, m.cy }), marks != nullptr ? marks->of (this) : Mark::None, true);
}

//==============================================================================
namespace
{
// the faders (GUIXML): their movement areas and caps; a cap's top moves from the area's top (Max) over the area's
// height less the cap's (measured: floor of the position)
struct FaderGeom { int x, top, height; const sk::LowRank* cap; const sk::LowRank* shadow; float lo, hi; };
const FaderGeom& geom (Fader::Kind k)
{
    static const FaderGeom g[] = { { 231, 219, 236, &sk::outputCap, &sk::outputShadow, -12.0f, 12.0f },
                                   { 28, 204, 233, &sk::rangeMaxCap, &sk::rangeMaxShadow, -11.0f, 12.0f },
                                   { 28, 234, 233, &sk::rangeMinCap, &sk::rangeMinShadow, -12.0f, 11.0f },
                                   { 114, 197, 279, &sk::riderCap, &sk::riderShadow, -12.0f, 12.0f } };
    return g[(int) k];
}
}

Fader::Fader (Kind k) : kind (k)
{
    setSliderStyle (LinearVertical);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setVelocityModeParameters (1.0, 1, 0.0, false);
}

Rectangle<float> Fader::capBounds() const
{
    const auto& fg = geom (kind);
    const float travel = (float) (fg.height - fg.cap->h);
    const float y = std::floor ((float) fg.top + travel * (fg.hi - (float) getValue()) / (fg.hi - fg.lo));
    return { (float) (fg.x - getX()), y - (float) getY(), (float) fg.cap->w, (float) fg.cap->h };
}

// the cap's visible part (its image has transparent margins)
Rectangle<float> Fader::capCore() const
{
    const auto b = capBounds();
    switch (kind)
    {
        case Rider:    return b.reduced (8.0f, 3.0f);
        case Output:   return b.reduced (5.0f, 2.0f);
        default:       return b.reduced (7.0f, 2.0f);
    }
}

bool Fader::hitTest (int x, int y) { return capCore().expanded (2.0f).contains ((float) x, (float) y); }
void Fader::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void Fader::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }

void Fader::mouseDown (const MouseEvent& e)
{
    blocked = marks != nullptr && ! marks->click (this);
    if (blocked) return;
    if (kind == Rider && onGrab) onGrab (true);
    feel.down (*this, e);
}
void Fader::mouseDrag (const MouseEvent& e)
{
    if (blocked) return;
    const auto& fg = geom (kind);
    feel.drag (*this, e, (float) (fg.height - fg.cap->h) / (fg.hi - fg.lo), true);
}
void Fader::mouseUp (const MouseEvent& e)
{
    if (blocked) { blocked = false; return; }
    feel.up (*this, e);
    if (kind == Rider && onGrab) onGrab (false);
}

void Fader::paint (Graphics& g)
{
    const float s = physicalScale (g);
    const auto& fg = geom (kind);
    const auto b = capBounds();
    // the shadow (drawn first, at the cap's top left, as the reference's), then the cap
    drawSprite (g, sprite (shadow, s, [&fg] (float sc) { return renderLowRank (*fg.shadow, sc); }), s, b.getX(), b.getY());
    drawSprite (g, sprite (cap, s, [&fg] (float sc) { return renderLowRank (*fg.cap, sc); }), s, b.getX(), b.getY());
    drawMark (g, capCore(), marks != nullptr ? marks->of (this) : Mark::None);
}

//==============================================================================
TargetSlider::TargetSlider()
{
    setSliderStyle (LinearHorizontal);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setVelocityModeParameters (1.0, 1, 0.0, false);
}

// the control is 208 px wide (59 .. 267); the glass 31 px: its left edge from 0 (-42 dB) to 177 (0 dB), on whole pixels
float TargetSlider::thumbX (double v) const { return std::floor ((float) ((v + 42.0) / 42.0 * 177.0)); }
bool TargetSlider::hitTest (int x, int y) { return Rectangle<float> (thumbX (getValue()), 0.0f, 31.0f, 20.0f).expanded (1.0f).contains ((float) x, (float) y); }
void TargetSlider::mouseEnter (const MouseEvent&) { setMarks (marks, this, true); }
void TargetSlider::mouseExit (const MouseEvent&) { setMarks (marks, this, false); }
void TargetSlider::mouseDown (const MouseEvent& e)
{
    blocked = marks != nullptr && ! marks->click (this);
    if (! blocked) feel.down (*this, e);
}
void TargetSlider::mouseDrag (const MouseEvent& e) { if (! blocked) feel.drag (*this, e, 177.0f / 42.0f, false); }
void TargetSlider::mouseUp (const MouseEvent& e) { if (! blocked) feel.up (*this, e); blocked = false; }

void TargetSlider::paint (Graphics& g)
{
    const float s = physicalScale (g);
    // the vocal level meter under the glass: 184 x 12 at 12, 5 (local); -42 .. 0 dBFS from left to right
    const float lit01 = jlimit (0.0f, 1.0f, (levelDb + 42.0f) / 42.0f);
    drawSprite (g, sprite (track, s, [] (float sc) { return renderLowRank (sk::meterTrack, sc); }), s, 12.0f, 5.0f);
    if (lit01 > 0.0f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (12, 5, roundToInt (184.0f * lit01), 12);
        drawSprite (g, sprite (lit, s, [] (float sc) { return renderLowRank (sk::meterLit, sc); }), s, 12.0f, 5.0f);
    }
    const float x = thumbX (getValue());
    drawSprite (g, sprite (glass, s, [] (float sc) { return renderLowRank (sk::targetGlass, sc); }), s, x, 0.0f);
    drawMark (g, Rectangle<float> (x, 0.0f, 31.0f, 20.0f), marks != nullptr ? marks->of (this) : Mark::None);
}

//==============================================================================
// the Idle arrow (measured on the reference's image, drawn 6 px left of its control): a triangle, its colour by row
void IdleColumn::paint (Graphics& g)
{
    const float y = std::round (scaleY (idleDb ? idleDb() : 0.0f) - 5.5f) - (float) getY();
    const float x = 27.0f - (float) getX();
    Path tri;
    tri.addTriangle (x + 6.0f, y + 1.4f, x + 6.0f, y + 9.6f, x + 11.8f, y + 5.5f);
    ColourGradient cg (Colour ((uint8) sk::idleArrowRows[3], (uint8) sk::idleArrowRows[4], (uint8) sk::idleArrowRows[5]), 0.0f, y + 1.5f,
                       Colour ((uint8) sk::idleArrowRows[27], (uint8) sk::idleArrowRows[28], (uint8) sk::idleArrowRows[29]), 0.0f, y + 9.5f, false);
    g.setGradientFill (cg);
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
void Switch::paintButton (Graphics& g, bool, bool)
{
    const float s = physicalScale (g);
    const bool fast = getToggleState();
    drawSprite (g, sprite (states[fast ? 1 : 0], s, [fast] (float sc) { return renderLowRank (fast ? sk::switchFast : sk::switchSlow, sc); }), s, 0.0f, 0.0f);
    if (marks != nullptr && marks->of (this) != Mark::None)
    {
        g.setColour (kMarkSelected);
        g.drawRect (getLocalBounds(), 1);
    }
}

//==============================================================================
// the Automation buttons (132 x 26): the all-off picture, the chosen button's third from the all-lit one, the names
// printed in the colours measured on each
void AutomationButtons::paint (Graphics& g)
{
    const float s = physicalScale (g);
    const int chosen = mode ? mode() : 0;
    drawSprite (g, sprite (off, s, [] (float sc) { return renderLowRank (sk::automationOff, sc); }), s, 0.0f, 0.0f);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (chosen * 44, 0, 44, 26);
        drawSprite (g, sprite (on, s, [] (float sc) { return renderLowRank (sk::automationOn, sc); }), s, 0.0f, 0.0f);
    }
    for (int i = 0; i < 3; ++i)
    {
        const auto& l = (i == chosen ? sk::automationOnText : sk::automationOffText)[i];
        const float* rgb = (i == chosen ? sk::automationOnTextRgb : sk::automationOffTextRgb) + i * 3;
        GlyphArrangement ga;
        ga.addLineOfText (arial (12.0f, true), String (l.text), 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        const auto b = p.getBounds();
        const auto box = Rectangle<float>::leftTopRightBottom (l.x0, l.y0, l.x1, l.y1).expanded (0.2f);
        p.applyTransform (AffineTransform::fromTargetPoints (b.getX(), b.getY(), box.getX(), box.getY(), b.getRight(), b.getY(), box.getRight(), box.getY(),
                                                              b.getX(), b.getBottom(), box.getX(), box.getBottom()));
        g.setColour (Colour ((uint8) jlimit (0.0f, 255.0f, rgb[0]), (uint8) jlimit (0.0f, 255.0f, rgb[1]), (uint8) jlimit (0.0f, 255.0f, rgb[2])));
        g.fillPath (p);
    }
}

void AutomationButtons::mouseDown (const MouseEvent& e)
{
    if (setMode) setMode (jlimit (0, 2, (int) (e.position.x / 44.0f)));
    repaint();
}

//==============================================================================
WVoxRiderEditor::WVoxRiderEditor (WVoxRiderProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wvoxrider::mode), "WVox Rider", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Renaissance, wvoxrider::modeNames()),
      lasso (make<wavy::flat::LassoOverlay>()),
      proc (p),
      vocal (make<Knob> (false)), music (make<Knob> (false)), spill (make<Knob> (true)), attackSwitch (make<Switch>()),
      target (make<TargetSlider>()), rangeMax (make<Fader> (Fader::RangeMax)), rangeMin (make<Fader> (Fader::RangeMin)),
      output (make<Fader> (Fader::Output)), rider (make<Fader> (Fader::Rider)), idleColumn (make<IdleColumn>()),
      automation (make<AutomationButtons>())
{
    setFactoryPresets (wvoxrider::factoryPresets());
    marks.blueAlone = { &rider, &idleColumn };
    marks.changed = [this] { content().repaint(); };
    marks.enter = [this] (Slider* sl) { openEntry (sl); };
    for (auto* k : { &vocal, &music, &spill }) k->marks = &marks;
    for (auto* f : { &rangeMax, &rangeMin, &output, &rider }) f->marks = &marks;
    target.marks = &marks;
    attackSwitch.marks = &marks;

    attachSlider (vocal, wvoxrider::vocalSensitivity, kStudio.vocal, 0.0);
    attachSlider (music, wvoxrider::musicSensitivity, kStudio.music, 0.0);
    attachSlider (spill, wvoxrider::spill, kLive.spill, -48.0);
    attach (attackSwitch, wvoxrider::attack, kStudio.attack);
    attachSlider (target, wvoxrider::target, { 59, 42, 208, 20 }, -21.0);
    // the faders' components span the column; their caps move inside
    attachSlider (rangeMax, wvoxrider::rangeMax, { 13, 195, 82, 305 }, 6.0);
    attachSlider (rangeMin, wvoxrider::rangeMin, { 13, 195, 82, 305 }, -6.0);
    attachSlider (output, wvoxrider::output, { 225, 205, 72, 290 }, 0.0);
    // the Rider shows the ride (the processor's gain) and can be held by hand; it is the host's Rider Fader parameter
    // only through Automation (Write sends the ride, Read plays the host's back)
    rider.setRange (-12.0, 12.0, 0.1);
    addToContent (rider, { 110, 190, 100, 300 });
    rider.onGrab = [this] (bool held)
    {
        proc.userRideDb.store ((float) rider.getValue());
        proc.userRide.store (held);
        if (! held) content().repaint();
    };
    rider.onValueChange = [this] { if (proc.userRide.load()) proc.userRideDb.store ((float) rider.getValue()); content().repaint (135, 470, 50, 26); };
    // Range Max stays at least 1 dB above Min (the Effective Range is 1 .. 24 dB)
    rangeMax.onValueChange = [this] { if (rangeMin.getValue() > rangeMax.getValue() - 1.0) rangeMin.setValue (rangeMax.getValue() - 1.0, sendNotificationSync); content().repaint(); };
    rangeMin.onValueChange = [this] { if (rangeMax.getValue() < rangeMin.getValue() + 1.0) rangeMax.setValue (rangeMin.getValue() + 1.0, sendNotificationSync); content().repaint(); };
    output.onValueChange = [this] { content().repaint (230, 470, 60, 26); };
    for (auto* k : { &vocal, &music, &spill })
        k->onValueChange = [this, k] { content().repaint (k->getX() - 10, k->getY() + 60, k->getWidth() + 20, 30); };
    target.onValueChange = [this] { content().repaint (266, 40, 50, 26); };
    // the Idle arrow
    auto* idleParam = p.apvts.getParameter (wvoxrider::idle);
    idleColumn.marks = &marks;
    idleColumn.idleDb = [this] { return proc.idleDb(); };
    idleColumn.setIdleDb = [this, idleParam] (float db)
    {
        const float lo = proc.get (wvoxrider::rangeMin), hi = proc.get (wvoxrider::rangeMax);
        idleParam->setValueNotifyingHost (idleParam->convertTo0to1 (jlimit (0.02f, 0.98f, (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo))));
        content().repaint();
    };
    idleColumn.gesture = [idleParam] (bool begin) { if (begin) idleParam->beginChangeGesture(); else idleParam->endChangeGesture(); };
    idleColumn.reset = [this, idleParam] { idleParam->setValueNotifyingHost (idleParam->getDefaultValue()); content().repaint(); };
    addToContent (idleColumn, { 27, 224, 18, 224 });
    // the Automation buttons
    auto* autoParam = p.apvts.getParameter (wvoxrider::automation);
    automation.marks = &marks;
    automation.mode = [this] { return proc.automationMode(); };
    automation.setMode = [autoParam] (int m) { autoParam->beginChangeGesture(); autoParam->setValueNotifyingHost (autoParam->convertTo0to1 ((float) m)); autoParam->endChangeGesture(); };
    addToContent (automation, { 94, 522, 132, 26 });
    addToContent (lasso, { 0, 0, W, H });
    lasso.setVisible (false);
    layoutControls();
}

WVoxRiderEditor::~WVoxRiderEditor()
{
    entry.reset();
    for (auto* k : { &vocal, &music, &spill }) k->marks = nullptr;
}

// Studio / Live: the knobs, the switch and the LEDs move; Spill shows on Live only
void WVoxRiderEditor::layoutControls()
{
    const bool live = wvoxrider::isLive (proc.modeIndex());
    if ((int) live == laidOut) return;
    laidOut = (int) live;
    const auto& l = live ? kLive : kStudio;
    vocal.setBounds (l.vocal);
    music.setBounds (l.music);
    attackSwitch.setBounds (l.attack);
    spill.setVisible (live);
    content().repaint();
}

Rectangle<int> WVoxRiderEditor::entryArea (Slider* sl) const
{
    const auto& l = wvoxrider::isLive (proc.modeIndex()) ? kLive : kStudio;
    if (sl == &vocal) return l.vocalValue;
    if (sl == &music) return l.musicValue;
    if (sl == &spill) return l.spillValue;
    if (sl == &target) return { 272, 45, 32, 13 };
    if (sl == &output) return { 239, 476, 36, 15 };
    if (sl == &rangeMax || sl == &rangeMin)
    {
        auto* f = static_cast<Fader*> (sl);
        const auto cap = f->capCore().translated ((float) f->getX(), (float) f->getY());
        return { 88, roundToInt (cap.getCentreY()) - 7, 31, 14 };
    }
    return {};
}

void WVoxRiderEditor::openEntry (Slider* sl)
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

void WVoxRiderEditor::closeEntry (bool apply)
{
    if (entry == nullptr) return;
    auto e = std::move (entry);
    auto* sl = entryFor;
    entryFor = nullptr;
    if (apply && sl != nullptr)
    {
        const auto t = e->getText().trim().upToFirstOccurrenceOf (" ", false, false);
        if (t.containsAnyOf ("0123456789"))
        {
            double v = t.getDoubleValue();
            if (sl == &spill) v = -std::abs (v);
            if (v >= sl->getMinimum() - 1.0e-9 && v <= sl->getMaximum() + 1.0e-9)
            {
                Slider::ScopedDragNotification drag (*sl);
                sl->setValue (v, sendNotificationSync);
            }
        }
    }
    e->setVisible (false);
    MessageManager::callAsync ([e = std::shared_ptr<TextEditor> (std::move (e))] {});
    content().repaint();
}

void WVoxRiderEditor::contentMouseDown (const MouseEvent& e)
{
    if (Rectangle<float> (280.0f, 192.0f, 15.0f, 15.0f).contains (e.position))   // the clip LED: a click clears it
    {
        clip = false;
        content().repaint (278, 190, 20, 20);
        return;
    }
    lassoing = true;
    lassoStart = e.position;
    marks.group.clear();
    marks.selected = nullptr;
    if (marks.changed) marks.changed();
}

void WVoxRiderEditor::contentMouseDrag (const MouseEvent& e)
{
    if (! lassoing) return;
    const auto r = Rectangle<float> (lassoStart, e.position);
    lasso.rect = r.toNearestInt();
    lasso.setVisible (true);
    lasso.repaint();
    auto at = [] (Component& c, Rectangle<float> local) { return local.translated ((float) c.getX(), (float) c.getY()); };
    std::vector<std::pair<Component*, Rectangle<float>>> items = {
        { &target, at (target, { target.thumbX (target.getValue()), 0.0f, 31.0f, 20.0f }) }, { &attackSwitch, attackSwitch.getBounds().toFloat() },
        { &vocal, vocal.getBounds().toFloat() }, { &music, music.getBounds().toFloat() },
        { &rangeMax, at (rangeMax, rangeMax.capCore()) }, { &idleColumn, Rectangle<float> (33.0f, scaleY (proc.idleDb()) - 5.5f, 12.0f, 11.0f) },
        { &rangeMin, at (rangeMin, rangeMin.capCore()) }, { &rider, at (rider, rider.capCore()) }, { &output, at (output, output.capCore()) } };
    if (spill.isVisible()) items.insert (items.begin() + 2, { &spill, spill.getBounds().toFloat() });
    marks.group.clear();
    for (auto& [c, b] : items)
        if (b.intersects (r)) marks.group.push_back (c);
    marks.selected = marks.group.empty() ? nullptr : marks.group.front();
    if (marks.changed) marks.changed();
}

void WVoxRiderEditor::contentMouseUp (const MouseEvent&)
{
    lassoing = false;
    lasso.setVisible (false);
    if (marks.group.size() < 2) marks.group.clear();
    if (marks.changed) marks.changed();
}

void WVoxRiderEditor::tick()
{
    layoutControls();
    const double now = Time::getMillisecondCounterHiRes() * 0.001, dt = lastTick > 0.0 ? now - lastTick : 0.0;
    lastTick = now;
    // the vocal level meter (provisional ballistics, as WBass Rider's Target meter: up at once, down 37.3 dB/s at most)
    const float e = proc.vocalDb.take();
    level = e >= level ? e : jmax (e, level - 37.3f * (float) jmin (dt, 0.25));
    target.levelDb = level;
    target.repaint();
    if (proc.outDb.take() >= 0.0f && ! clip) { clip = true; content().repaint (278, 190, 20, 20); }
    const bool sc = proc.sideChainOn.load();
    if (sc != sideChain) { sideChain = sc; music.disabledLook = ! sc; music.repaint(); }
    music.disabledLook = ! sideChain;
    // the Music LED: brighter as the music is louder (provisional: -40 .. -10 dBFS)
    const float m = sideChain ? jlimit (0.0f, 1.0f, (proc.musicDb.take() + 40.0f) / 30.0f) : 0.0f;
    if (std::abs (m - music01) > 0.02f) { music01 = m; content().repaint(); }
    const bool rd = proc.riding.load();
    if (rd != riding) { riding = rd; content().repaint(); }
    const float r = proc.rideDb.load();
    if (! rider.isMouseButtonDown() && std::abs (r - ride) > 1.0e-4f)
    {
        ride = r;
        rider.setValue (r, dontSendNotification);
    }
    content().repaint (100, 190, 110, 310);
    idleColumn.repaint();
}

//==============================================================================
// the printed art, in design coordinates (WVoxRiderSkinData.h)
void WVoxRiderEditor::paintStatic (Graphics& g, bool live) const
{
    const auto& lay = live ? kLive : kStudio;
    // ---- top panel
    auto labels = [&g] (const auto& list) { for (const auto& l : list) drawLabel (g, l); };
    if (live) labels (sk::labelsLive); else labels (sk::labels);
    if (live) drawRuns (g, sk::linesTopLive, std::size (sk::linesTopLive));
    else drawRuns (g, sk::linesTop, std::size (sk::linesTop));
    auto centre = [] (Rectangle<int> r) { return Point<float> ((float) r.getX() + sk::bigKnob.cx, (float) r.getY() + sk::bigKnob.cy); };
    drawScale (g, sk::bigScale, centre (lay.vocal));
    drawScale (g, sk::bigScale, centre (lay.music));
    if (live) drawScale (g, sk::spillScale, { (float) lay.spill.getX() + sk::spillKnob.cx, (float) lay.spill.getY() + sk::spillKnob.cy });

    // ---- silver panel: labels, the Automation brackets, the lamps
    for (const auto& l : sk::labelsSilver) drawLabel (g, l);
    drawRuns (g, sk::linesSilver, std::size (sk::linesSilver));
    // the wells behind the tracks (measured: 21.5 px wide, y 210 .. 463, the panel darkened by wellShade, round ends)
    for (float x : { 51.0f, 149.5f, 247.0f })
    {
        g.setColour (Colours::black.withAlpha (1.0f - sk::wellShade));
        g.fillRoundedRectangle (x, 210.0f, 21.5f, 253.0f, 4.0f);
    }
    // the tracks (measured)
    g.setColour (Colours::black);
    g.fillRect (Rectangle<float>::leftTopRightBottom (60.0f, 218.0f, 62.45f, 454.45f));
    g.fillRect (Rectangle<float>::leftTopRightBottom (157.0f, 218.0f, 162.0f, 456.0f));
    g.fillRect (Rectangle<float>::leftTopRightBottom (256.0f, 223.0f, 258.45f, 450.0f));
    // the scales: a mark every 3 dB (2 px at +-12 and 0, else 1 px), five half-dark ticks between marks; the Rider's on
    // both sides (19 px marks, 8 px ticks), the Output's on its right (11 / 6 px marks, 4 px ticks)
    for (int k = 0; k <= 8; ++k)
    {
        const float yk = 231.0f + 26.25f * (float) k;
        const bool thick = k % 4 == 0;
        const float top = thick ? yk - 1.0f : std::floor (yk), h = thick ? 2.0f : 1.0f;
        g.setColour (kScaleInk);
        g.fillRect (129.0f, top, 19.0f, h);
        g.fillRect (172.0f, top, 19.0f, h);
        g.fillRect (270.0f, top, thick ? 11.0f : 6.0f, h);
        if (k == 8) break;
        g.setColour (kScaleInk.withAlpha (0.55f));
        for (int j = 0; j < 5; ++j)
        {
            const float ty = (thick ? yk + 5.5f : top + 5.0f) + 4.0f * (float) j;
            g.fillRect (140.0f, ty, 8.0f, 1.0f);
            g.fillRect (172.0f, ty, 8.0f, 1.0f);
            g.fillRect (270.0f, ty, 4.0f, 1.0f);
        }
    }
    // the value boxes (measured: rounded black boxes, rows 476.2 .. 490.8, a soft dark row above and below them)
    for (auto [x0, x1] : { std::pair<float, float> { 44.55f, 79.45f }, { 142.55f, 177.45f }, { 239.3f, 274.7f } })
    {
        g.setColour (kValueBox.withAlpha (0.27f));
        g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (x0, 475.0f, x1, 492.0f), 2.5f);
        g.setColour (kValueBox);
        g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (x0, 476.22f, x1, 490.78f), 1.5f);
    }

    // ---- the name plate: fine light scratches on the leather (as WBass Rider's, denser: the reference's leather here is
    // scratched all over), the plug-in's name in a bold serif, silver, right-aligned where the reference's ends
    {
        Random rnd (20261028);
        for (int i = 0; i < 520; ++i)
        {
            const float x = rnd.nextFloat() * 320.0f, y = 552.0f + rnd.nextFloat() * 46.0f;
            const float ang = degreesToRadians ((rnd.nextBool() ? 62.0f : 118.0f) + (rnd.nextFloat() - 0.5f) * 40.0f);
            const float len = 3.0f + rnd.nextFloat() * 18.0f;
            g.setColour (Colours::white.withAlpha (0.04f + 0.10f * rnd.nextFloat()));
            g.drawLine (x, y, x + len * std::cos (ang), jlimit (552.0f, 597.0f, y + len * std::sin (ang)), 0.6f);
        }
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions ("Georgia", 20.0f, Font::bold)), "WVox Rider", 0.0f, 0.0f);
        Path text;
        ga.createPath (text);
        const auto tb = text.getBounds();
        const float k = 14.2f / tb.getHeight();                         // the reference's lettering: 567 .. 581
        const float right = live ? 269.0f : 303.0f;
        text.applyTransform (AffineTransform::scale (k).translated (right - tb.getRight() * k, 581.2f - tb.getBottom() * k));
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillPath (text, AffineTransform::translation (0.8f, 1.0f));
        ColourGradient silver (Colour (240, 240, 240), 0.0f, 567.0f, Colour (160, 160, 160), 0.0f, 581.0f, false);
        g.setGradientFill (silver);
        g.fillPath (text);
        if (live)
        {
            // the badge: a dark box in a grey frame, LIVE in yellow (measured on the Live background)
            const Rectangle<float> b (276.0f, 565.0f, 37.0f, 19.0f);
            g.setColour (Colour (20, 20, 20));
            g.fillRect (b);
            g.setColour (Colour (110, 110, 110));
            g.drawRect (b, 1.0f);
            const sk::Label l { "LIVE", 280.0f, 569.0f, 308.0f, 580.0f, 0.0f, true };
            GlyphArrangement gl;
            gl.addLineOfText (arial (12.0f, true), "LIVE", 0.0f, 0.0f);
            Path p;
            gl.createPath (p);
            const auto pb = p.getBounds();
            const auto box = Rectangle<float>::leftTopRightBottom (l.x0, l.y0, l.x1, l.y1);
            p.applyTransform (AffineTransform::fromTargetPoints (pb.getX(), pb.getY(), box.getX(), box.getY(), pb.getRight(), pb.getY(), box.getRight(), box.getY(),
                                                                  pb.getX(), pb.getBottom(), box.getX(), box.getBottom()));
            g.setColour (Colour (255, 242, 0));
            g.fillPath (p);
        }
    }
}

void WVoxRiderEditor::paintContent (Graphics& g)
{
    const bool live = wvoxrider::isLive (proc.modeIndex());
    const float s = physicalScale (g);
    if (background.isNull() || std::abs (s - backgroundScale) > 1.0e-3f || backgroundLive != (int) live)
    {
        backgroundScale = s;
        backgroundLive = (int) live;
        const int bw = roundToInt (W * s), bh = roundToInt (H * s);
        background = Image (Image::RGB, bw, bh, true);
        {
            Image::BitmapData bd (background, Image::BitmapData::writeOnly);
            // a panel's colour at (x, y): rows times columns times its correction (linear between cell centres)
            auto panelAt = [] (const sk::Panel& p, float x, float y, float rgb[3])
            {
                const int r = jlimit (0, p.h - 1, (int) (y - (float) p.y0)), c = jlimit (0, p.w - 1, (int) (x - (float) p.x0));
                const float fx = jlimit (0.0f, (float) p.fixW - 1.001f, (x - p.fx0) / p.step), fy = jlimit (0.0f, (float) p.fixH - 1.001f, (y - p.fy0) / p.step);
                const int i = (int) fx, j = (int) fy;
                const float tx = fx - (float) i, ty = fy - (float) j;
                auto at = [&p] (int jj, int ii) { return p.fix[jj * p.fixW + ii]; };
                const float f = (at (j, i) * (1.0f - tx) + at (j, i + 1) * tx) * (1.0f - ty) + (at (j + 1, i) * (1.0f - tx) + at (j + 1, i + 1) * tx) * ty;
                for (int k = 0; k < 3; ++k) rgb[k] = p.rows[r * 3 + k] * p.cols[c] * f;
            };
            const auto& top = live ? sk::topPanelLive : sk::topPanel;
            for (int py = 0; py < bh; ++py)
                for (int px = 0; px < bw; ++px)
                {
                    const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                    float rgb[3] = { 0.0f, 0.0f, 0.0f };
                    float grain = 0.0f;
                    if (y >= 36.0f && y < 171.0f && x >= 6.0f && x < 314.0f) { panelAt (top, x, y, rgb); grain = top.grain; }
                    else if (y >= 171.0f && y < 551.0f && x >= 4.0f && x < 316.0f) { panelAt (sk::silverPanel, x, y, rgb); grain = sk::silverPanel.grain; }
                    else if (y >= 551.0f) { panelAt (sk::leatherPanel, x, y, rgb); grain = 0.4f * sk::leatherPanel.grain; }   // (the scratches carry the rest)
                    // a fine grain, the rest of what the models leave (deterministic per design pixel)
                    uint32 hsh = (uint32) x * 374761393u + (uint32) y * 668265263u;
                    hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
                    hsh ^= hsh >> 16;
                    const float n = 0.8f * grain * ((float) (hsh & 0xffff) / 32767.5f - 1.0f);
                    bd.setPixelColour (px, py, Colour ((uint8) jlimit (0.0f, 255.0f, rgb[0] + n), (uint8) jlimit (0.0f, 255.0f, rgb[1] + n),
                                                       (uint8) jlimit (0.0f, 255.0f, rgb[2] + n)));
                }
        }
        Graphics bg (background);
        bg.addTransform (AffineTransform::scale (s));
        paintStatic (bg, live);
        // the lamps (fitted on the background)
        for (auto [m, x, y] : { std::tuple<const sk::Polar*, float, float> { &sk::lampTop, sk::lampTopX, sk::lampTopY },
                                std::tuple<const sk::Polar*, float, float> { &sk::lampBottom, sk::lampBottomX, sk::lampBottomY } })
        {
            const auto img = renderPolar (*m, 13, 13, s, 0.0f, 6.5f);
            Graphics::ScopedSaveState ss (bg);
            bg.addTransform (AffineTransform::scale (1.0f / s));
            bg.drawImageAt (img, roundToInt (x * s), roundToInt (y * s));
        }
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.drawImageAt (background, 0, 0);
    }
    const auto& lay = live ? kLive : kStudio;

    // the Range band: between Min and Max on the Rider's scale, a warm light over the panel and the Rider's left marks
    // (measured, frame over background: (255, 255, 215) at 0.165, x 65 .. 147.5); the Idle line across it, 2 px of a
    // red at 0.35
    const float lo = proc.get (wvoxrider::rangeMin), hi = proc.get (wvoxrider::rangeMax);
    g.setColour (Colour (255, 255, 215).withAlpha (0.165f));
    g.fillRect (Rectangle<float>::leftTopRightBottom (65.0f, scaleY (hi), 147.5f, scaleY (lo)));
    g.setColour (kIdleLine.withAlpha (0.35f));
    g.fillRect (65.0f, scaleY (proc.idleDb()) - 1.0f, 82.5f, 2.0f);
    if (idleColumn.dragging || marks.hover == &idleColumn || marks.showsValue (&idleColumn)) hoverBox (g, { 3.0f, scaleY (proc.idleDb()) - 6.5f, 27.0f, 13.0f }, String (proc.idleDb(), 1));

    // the LEDs: Ride / Idle (17 x 36: Ride above), the clip LED, the Music LED (between its off and lit pictures)
    auto led = [&] (Sprite& sp, const sk::Polar& m, int w, int h, float x, float y)
    {
        drawSprite (g, sprite (sp, s, [&m, w, h] (float sc) { return renderPolar (m, w, h, sc, 0.0f, (float) jmax (w, h)); }), s, x, y);
    };
    led (leds[riding ? 0 : 1], riding ? sk::ledRideOn : sk::ledRideOff, sk::ledRideOnW, sk::ledRideOnH, (float) lay.leds.getX(), (float) lay.leds.getY());
    led (leds[riding ? 2 : 3], riding ? sk::ledIdleOff : sk::ledIdleOn, sk::ledIdleOnW, sk::ledIdleOnH, (float) lay.leds.getX(), (float) lay.leds.getY() + 18.0f);
    led (clipLed[clip ? 1 : 0], clip ? sk::clipOn : sk::clipOff, 15, 15, 280.0f, 192.0f);
    led (musicLed[0], sk::musicLedOff, sk::musicLedOffW, sk::musicLedOffH, (float) lay.musicLed.getX(), (float) lay.musicLed.getY());
    if (music01 > 0.0f)
    {
        Graphics::ScopedSaveState ss (g);
        g.setOpacity (music01);
        const auto& img = sprite (musicLed[1], s, [] (float sc) { return renderPolar (sk::musicLedOn, sk::musicLedOnW, sk::musicLedOnH, sc, 0.0f, 40.0f); });
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.drawImageAt (img, roundToInt ((float) lay.musicLed.getX() * s), roundToInt ((float) lay.musicLed.getY() * s));
    }

    // value boxes
    valueBox (g, { 44.55f, 475.3f, 34.9f, 16.4f }, String (hi - lo, 1));
    valueBox (g, { 142.55f, 475.3f, 34.9f, 16.4f }, String (rider.getValue(), 1));
    valueBox (g, { 239.3f, 475.3f, 35.4f, 16.4f }, String (output.getValue(), 1));

    // the values shown under the mouse
    if (target.isMouseButtonDown() || marks.showsValue (&target)) hoverBox (g, { 272.0f, 45.0f, 32.0f, 13.0f }, String (target.getValue(), 1));
    for (auto* f : { &rangeMax, &rangeMin })
        if (f->isMouseButtonDown() || marks.showsValue (f))
        {
            const auto cap = f->capCore().translated ((float) f->getX(), (float) f->getY());
            hoverBox (g, { 88.0f, cap.getCentreY() - 7.0f, 31.0f, 14.0f }, String (f->getValue(), 1));
        }
    const std::pair<Knob*, Rectangle<int>> knobs[] = { { &vocal, lay.vocalValue }, { &music, lay.musicValue }, { &spill, lay.spillValue } };
    for (auto [k, r] : knobs)
        if (k->isVisible() && k->showing() && ! r.isEmpty())
            hoverBox (g, r.toFloat(), k == &music && wvoxrider::musicOff ((float) k->getValue()) ? String ("Off") : String (k->getValue(), 1));
}
