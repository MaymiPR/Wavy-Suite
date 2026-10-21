#include "WavyHybridUI.h"
#include "WavyHybridKnobData.h"
#include "WavyLogo.h"
#include <cmath>

using namespace juce;

namespace wavy::hybrid
{
Font font (float h, bool bold) { return Font (FontOptions ("Arial", h, bold ? Font::bold : Font::plain)); }

static float rad (float deg) { return deg * MathConstants<float>::pi / 180.0f; }
// the reference's angles: 0 = straight down, clockwise; JUCE: 0 = up, clockwise
static float refToJuce (float deg) { return rad (deg + 180.0f); }

//==============================================================================
// The panel, measured on the reference: a flat orange plate (187, 105, 39) brightened along the top (1.115 at y 28,
// fading to 1 by y 170), darkened along the bottom (from y 385 to 0.64 at the edge) and toward the left and right
// edges (0.76 at the outer columns, 1 by about 50 px in), with a 1 px dark border and a faint fixed grain.
static const Image& panelImage (Image& img, int w, int h)
{
    if (img.isNull() || img.getWidth() != w || img.getHeight() != h)
    {
        img = Image (Image::RGB, w, h, false);
        Random r (1234);
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        auto vert = [h] (float y)
        {
            y += 19.0f;                                         // design y (the panel starts under the toolbar)
            if (y < 28.0f) return 1.115f;
            if (y < 170.0f) return 1.115f - 0.115f * (y - 28.0f) / 142.0f;
            if (y < 385.0f) return 1.0f;
            const float t = (y - 385.0f) / 69.0f;
            return 1.0f - 0.36f * t * t;
        };
        auto horiz = [w] (float x)
        {
            const float d = std::min (x, (float) w - 1.0f - x);
            if (d < 1.0f) return 0.2f;
            if (d >= 52.0f) return 1.0f;
            const float t = d / 52.0f;
            return 0.76f + 0.24f * std::sqrt (t);
        };
        for (int y = 0; y < h; ++y)
        {
            const float v = vert ((float) y);
            for (int x = 0; x < w; ++x)
            {
                const float k = v * horiz ((float) x) * (1.0f + (r.nextFloat() - 0.5f) * 0.035f);
                bd.setPixelColour (x, y, Colour ((uint8) jlimit (0.0f, 255.0f, 187.0f * k), (uint8) jlimit (0.0f, 255.0f, 105.0f * k),
                                                 (uint8) jlimit (0.0f, 255.0f, 39.0f * k)));
            }
        }
    }
    return img;
}

void drawPanel (Graphics& g, Rectangle<int> area, Image& cache)
{
    g.setOpacity (1.0f);
    g.drawImageAt (panelImage (cache, area.getWidth(), area.getHeight()), area.getX(), area.getY());
}

void drawScale (Graphics& g, Point<float> c, float a0, float a1, const Scale& s)
{
    g.setColour (kPrint);
    for (int i = 0; i < s.ticks; ++i)
    {
        const float a = s.ticks > 1 ? a0 + (a1 - a0) * (float) i / (float) (s.ticks - 1) : a0;
        const Point<float> d (std::sin (a), -std::cos (a));
        g.drawLine (Line<float> (c + d * s.rIn, c + d * s.rOut), s.thick);
    }
    g.setFont (font (s.labelSize));
    for (const auto& l : s.labels)
    {
        const float a = s.ticks > 1 ? a0 + (a1 - a0) * (float) l.tick / (float) (s.ticks - 1) : a0;
        const Point<float> p = c + Point<float> (std::sin (a), -std::cos (a)) * l.radius + Point<float> (l.dx, l.dy);
        g.drawText (l.text, Rectangle<float> (p.x - 20.0f, p.y - 7.0f, 40.0f, 14.0f), Justification::centred);
    }
}

void printText (Graphics& g, const String& t, Point<float> centre, float em, float hs, bool black, float angle)
{
    const Font f = Font (FontOptions (black ? "Arial Black" : "Arial", 10.0f, black ? Font::plain : Font::bold).withPointHeight (em))
                       .withHorizontalScale (hs);
    GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    Path ink;
    ga.createPath (ink);
    const auto b = ink.getBounds();
    g.fillPath (ink, AffineTransform::translation (-b.getCentreX(), -b.getCentreY()).rotated (angle).translated (centre));
}

//==============================================================================
Knob::Knob (Kind k, float startDeg, float endDeg) : kind (k)
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (TextBoxBelow, false, 37, 13);      // laid out on the value slot by HybridLook, invisible until typing
    setRotaryParameters (refToJuce (startDeg), refToJuce (endDeg), true);
    setMouseDragSensitivity (500);
    setVelocityModeParameters (1.0, 1, 0.0, false);
    setScrollWheelEnabled (true);
}

float Knob::pointerAngle() const
{
    const auto rp = getRotaryParameters();
    float frac = (float) const_cast<Knob*> (this)->valueToProportionOfLength (getValue());
    if (steps > 1) frac = std::round (frac * (float) (steps - 1)) / (float) (steps - 1);
    return rp.startAngleRadians + frac * (rp.endAngleRadians - rp.startAngleRadians);
}

bool Knob::hitTest (int x, int y) { return centre.getDistanceFrom ({ (float) x, (float) y }) <= radius() + 5.0f; }

void Knob::mouseDown (const MouseEvent& e)
{
    if (selection != nullptr) selection->select (this);
    if (! feel.down (*this, e, {}, selection)) Slider::mouseDown (e);
}
void Knob::mouseDrag (const MouseEvent& e) { if (feel.active) feel.drag (*this, e, selection); else Slider::mouseDrag (e); }
void Knob::mouseUp (const MouseEvent& e)
{
    if (! feel.up (*this, e, centre, selection)) Slider::mouseUp (e);
}
void Knob::mouseDoubleClick (const MouseEvent&) { showTextBox(); }

namespace kd = knobdata;

static const kd::Kind& knobData (Knob::Kind k) { return k == Knob::Small ? kd::kSmall : k == Knob::Large ? kd::kLarge : kd::kHuge; }

// the angular basis of the shading model (tools/fit_hybrid_knobs.py): 1, cos/sin 1..3, the highlight lobes, the flutes
static void knobBasis (float th, float lobeTh, float* b)
{
    const float c1 = std::cos (th), s1 = std::sin (th), c2 = c1 * c1 - s1 * s1, s2 = 2.0f * s1 * c1;
    const float d = std::remainder (th - kd::kLight, MathConstants<float>::twoPi);
    const float g11 = std::exp (-square (d / rad (11.0f))), g28 = std::exp (-square (d / rad (28.0f))), g40 = std::exp (-square (d / rad (40.0f)));
    const float cl = std::cos ((float) kd::kLobes * lobeTh), sl = std::sin ((float) kd::kLobes * lobeTh);
    b[0] = 1.0f; b[1] = c1; b[2] = s1; b[3] = c2; b[4] = s2; b[5] = c2 * c1 - s2 * s1; b[6] = s2 * c1 + c2 * s1;
    b[7] = g11; b[8] = g28; b[9] = cl; b[10] = sl; b[11] = g40 * cl; b[12] = g40 * sl;
}

static void knobTable (const kd::Table& t, float coord, const float* b, float* rgb)
{
    const float fi = jlimit (0.0f, (float) (t.n - 1), (coord - t.lo) / t.step);
    const int i0 = jmin ((int) fi, t.n - 2);
    const float f = fi - (float) i0;
    for (int ch = 0; ch < 3; ++ch)
    {
        const int16_t* c0 = t.c + (i0 * 3 + ch) * kd::kBasis;
        const int16_t* c1 = c0 + 3 * kd::kBasis;
        float v0 = 0.0f, v1 = 0.0f;
        for (int k = 0; k < kd::kBasis; ++k) { v0 += (float) c0[k] * b[k]; v1 += (float) c1[k] * b[k]; }
        rgb[ch] = (v0 + (v1 - v0) * f) * (1.0f / 8.0f);
    }
}

float Knob::paintExtent() const
{
    const auto& K = knobData (kind);
    return std::ceil (jmax (K.rs + 1.0f, std::hypot (K.shadow.dx, K.shadow.dy) + K.shadow.r + 3.0f * K.shadow.w)) + 1.0f;
}

// the knob body with its skirt and cast shadow, centred in an image of (2 * extent * scale) pixels (premultiplied)
const Image& Knob::bodyImage (float scale, float delta, float& extent)
{
    const auto& K = knobData (kind);
    const float sdx = K.shadow.dx, sdy = K.shadow.dy, sr = K.shadow.r, sw = K.shadow.w, ss = K.shadow.s;
    extent = std::ceil (jmax (K.rs + 1.0f, std::hypot (sdx, sdy) + sr + 3.0f * sw));
    const bool full = body.isNull() || bodyScale != scale;
    if (! full && std::abs (delta - bodyDelta) < 0.0005f) return body;
    const int n = jmax (1, (int) std::ceil (2.0f * extent * scale));
    if (full) { bodyScale = scale; body = Image (Image::ARGB, n, n, true); }
    bodyDelta = delta;
    const int sub = scale < 1.5f ? 3 : 2;
    const float rimLo = K.emin - 2.0f;
    float eMax = K.edge[0];
    for (int k = 3; k < 9; ++k) eMax += std::abs (K.edge[k]);
    // after a turn only the fluted rim changes: from 8 px inside it (the fit's flute terms stop there) to past the
    // outermost flute
    const float ringLo = K.emin - 8.5f, ringHi = (K.skirt.n > 0 ? eMax : K.rs) + 1.5f;
    Random grain (77);
    std::vector<float> brush (512);                   // the skirt's brushed grain: a fixed value per thin ring
    for (auto& v : brush) v = (grain.nextFloat() - 0.5f) * 9.0f;
    Image::BitmapData bd (body, Image::BitmapData::readWrite);
    float b[kd::kBasis], rgb[3];
    for (int py = 0; py < n; ++py)
        for (int px = 0; px < n; ++px)
        {
            if (! full)
            {
                const float cx = ((float) px + 0.5f) / scale - extent, cy = ((float) py + 0.5f) / scale - extent;
                const float rc = std::sqrt (cx * cx + cy * cy);
                if (rc < ringLo || rc > ringHi) continue;
            }
            float A = 0.0f, R = 0.0f, G = 0.0f, B = 0.0f;
            for (int sy = 0; sy < sub; ++sy)
                for (int sx = 0; sx < sub; ++sx)
                {
                    const float x = ((float) px + ((float) sx + 0.5f) / (float) sub) / scale - extent;
                    const float y = ((float) py + ((float) sy + 0.5f) / (float) sub) / scale - extent;
                    const float r = std::sqrt (x * x + y * y);
                    float th = std::atan2 (x, -y);
                    if (th < 0.0f) th += MathConstants<float>::twoPi;
                    const float tk = th - delta;                                         // the angle on the turned knob
                    float e = K.edge[0];
                    for (int k = 1; k <= 3; ++k)
                        e += K.edge[2 * k + 1] * std::cos ((float) (k * kd::kLobes) * tk) + K.edge[2 * k + 2] * std::sin ((float) (k * kd::kLobes) * tk);
                    if (r < e || (K.skirt.n > 0 && r < K.rs))
                    {
                        knobBasis (th, tk, b);
                        if (r < rimLo) knobTable (K.inner, r, b, rgb);
                        else if (r < e) knobTable (K.rim, (r - rimLo) / (e - rimLo), b, rgb);
                        else
                        {
                            knobTable (K.skirt, r, b, rgb);
                            const float gr = brush[(size_t) jlimit (0, 511, (int) (r * 6.0f))];
                            rgb[0] += gr; rgb[1] += gr; rgb[2] += gr;
                        }
                        A += 1.0f; R += rgb[0]; G += rgb[1]; B += rgb[2];
                    }
                    else
                    {
                        const float rho = std::hypot (x - sdx, y - sdy);
                        A += ss * 0.5f * (1.0f - std::tanh ((rho - sr) / sw));      // black at this opacity
                    }
                }
            const float k = 1.0f / (float) (sub * sub), a = A * k;
            auto u8 = [] (float v) { return (uint8) jlimit (0.0f, 255.0f, v + 0.5f); };
            // premultiplied: the opaque samples' colour, scaled by their share
            *reinterpret_cast<PixelARGB*> (bd.getPixelPointer (px, py)) =
                PixelARGB (u8 (a * 255.0f), u8 (jmin (R * k, a * 255.0f)), u8 (jmin (G * k, a * 255.0f)), u8 (jmin (B * k, a * 255.0f)));
        }
    return body;
}

// the pointer: a dark fin with two facets lit by their angle to the light, a cream line along its ridge
static void fin (Graphics& g, Point<float> c, float a, Knob::Kind kind)
{
    // measured (reference/wcomp/knobs): where the ridge starts, the rim, the tip
    const float r0 = kind == Knob::Small ? 27.5f : kind == Knob::Large ? 40.5f : 57.0f;
    const float rr = kind == Knob::Small ? 31.6f : kind == Knob::Large ? 44.0f : 61.5f;
    const float rt = kind == Knob::Small ? 42.6f : kind == Knob::Large ? 59.6f : 82.0f;
    const float hw = 0.4f * (rt - rr), hw0 = kind == Knob::Small ? 1.8f : 2.4f;
    const Point<float> d (std::sin (a), -std::cos (a)), p (std::cos (a), std::sin (a));      // outward, and to the right
    auto at = [&] (float r, float w) { return c + d * r + p * w; };
    const Point<float> L (std::sin (kd::kLight), -std::cos (kd::kLight));
    const float lit = p.x * L.x + p.y * L.y;                                                   // > 0: the right facet faces the light

    // its soft shadow, cast down and to the right (measured: about 1.7, 5 px, 0.45 deep), turning with it
    {
        Path sp;
        sp.addTriangle (at (rr - 1.0f, -hw - 0.5f), at (rt + 0.5f, 0.0f), at (rr - 1.0f, hw + 0.5f));
        const auto off = kind == Knob::Small ? Point<int> (2, 5) : Point<int> (2, 4);
        DropShadow (Colours::black.withAlpha (0.5f), kind == Knob::Small ? 3 : 5, off).drawForPath (g, sp);
    }

    auto facet = [&] (float side, float shade)
    {
        Path f;
        f.startNewSubPath (at (r0, 0.0f));
        f.lineTo (at (r0, side * hw0));
        f.lineTo (at (rr, side * hw));
        f.lineTo (at (rt, 0.0f));
        f.closeSubPath();
        const auto v = (uint8) jlimit (0.0f, 253.0f, shade);
        const auto vt = (uint8) jlimit (0.0f, 253.0f, shade * 1.12f + 6.0f);
        ColourGradient fg (Colour (v, v, (uint8) (v + 2)), at (rr, 0.0f), Colour (vt, vt, (uint8) (vt + 2)), at (rt, 0.0f), false);
        g.setGradientFill (fg);
        g.fillPath (f);
    };
    facet (1.0f, 48.0f + 24.0f * lit);
    facet (-1.0f, 48.0f - 24.0f * lit);
    g.setColour (Colours::black.withAlpha (0.55f));
    Path outline;
    outline.startNewSubPath (at (rr, -hw)); outline.lineTo (at (rt, 0.0f)); outline.lineTo (at (rr, hw));
    g.strokePath (outline, PathStrokeType (0.6f));

    // the cream line on the ridge (about 2 px wide, measured)
    const float k = 0.92f + 0.08f * std::cos (a - rad (290.0f)), lw = kind == Knob::Small ? 1.0f : 1.1f;
    const Colour cream = kind == Knob::Small ? Colour (222, 208, 168) : Colour (240, 236, 205);
    Path line;
    line.startNewSubPath (at (r0 + 0.5f, -lw));
    line.lineTo (at (rt - 0.6f, -0.4f));
    line.lineTo (at (rt - 0.6f, 0.4f));
    line.lineTo (at (r0 + 0.5f, lw));
    line.closeSubPath();
    g.setColour (cream.withMultipliedBrightness (k));
    g.fillPath (line);
}

void Knob::paint (Graphics& g)
{
    const auto c = centre;
    const float scale = jlimit (0.5f, 4.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 4.0f) / 4.0f);
    float ext = 0.0f;
    // the reference's filmstrip turns the whole knob under a fixed light: the flutes are fitted in the knob's own frame,
    // turned by the pointer angle (tools/fit_hybrid_knobs.py)
    const float turn = std::remainder (pointerAngle(), MathConstants<float>::twoPi);
    const auto& img = bodyImage (scale, turn, ext);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (img, AffineTransform::scale (1.0f / scale).translated (c.x - ext, c.y - ext));
    fin (g, c, pointerAngle(), kind);
    // selected (measured): a thin yellow ring on the cap, and the value in a dark box on the value slot
    if (selection != nullptr && selection->contains (this))
    {
        const float rr = ringRadius();
        g.setColour (selection->is (this) ? Colour (222, 222, 6) : wavy::flat::kMember);
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.6f);
        if (! valueArea.isEmpty())
        {
            const auto v = valueArea.toFloat();
            g.setColour (Colour (90, 50, 18).withAlpha (0.8f));
            g.fillRect (v.translated (1.0f, 1.0f));
            g.setColour (Colour (65, 49, 36));
            g.fillRect (v);
            g.setColour (Colour (249, 248, 248));
            g.drawRect (v, 1.0f);
            g.setColour (Colour (205, 203, 200));
            g.setFont (font (11.0f, false));
            g.drawText (getTextFromValue (getValue()), v, Justification::centred);
        }
    }
}

//==============================================================================
ButtonGroup::ButtonGroup (RangedAudioParameter& p, UndoManager* um, StringArray l, bool v, bool t)
    : labels (std::move (l)), vertical (v), toggle (t)
{
    attachment = std::make_unique<ParameterAttachment> (p, [this] (float x) { selected = roundToInt (x); repaint(); }, um);
    attachment->sendInitialUpdate();
}

// The frame sits kShadow px inside the component, which also holds the frame's soft shadow on the panel.
static constexpr float kShadow = 4.0f;

Rectangle<float> ButtonGroup::buttonRect (int i) const
{
    // measured: a 1 px black frame, the buttons 1 px apart
    const auto b = getLocalBounds().toFloat().reduced (kShadow).reduced (1.0f);
    const int n = toggle ? 1 : labels.size();
    if (vertical)
    {
        const float h = (b.getHeight() - (float) (n - 1)) / (float) n;
        return { b.getX(), b.getY() + (float) i * (h + 1.0f), b.getWidth(), h };
    }
    const float w = (b.getWidth() - (float) (n - 1)) / (float) n;
    return { b.getX() + (float) i * (w + 1.0f), b.getY(), w, b.getHeight() };
}

void ButtonGroup::paint (Graphics& g)
{
    const auto f = getLocalBounds().toFloat().reduced (kShadow);
    for (int k = 4; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.07f));
        g.fillRoundedRectangle (f.translated (0.4f * (float) k, 0.9f * (float) k).expanded (0.5f * (float) k), 1.5f + (float) k);
    }
    g.setColour (Colour (2, 2, 2));
    g.fillRect (f);
    const int n = toggle ? 1 : labels.size();
    for (int i = 0; i < n; ++i)
    {
        const auto r = buttonRect (i);
        const bool on = toggle ? selected != 0 : selected == i;
        // the face's profile, top to bottom, measured on a 19 px button (as a share of the face colour)
        const Colour face = on ? Colour (163, 158, 137) : Colour (205, 203, 195);
        static const float upP[] = { 0.0f, 0.053f, 0.105f, 0.158f, 0.684f, 0.737f, 0.789f, 0.842f, 0.895f, 1.0f };
        static const float upV[] = { 0.51f, 0.93f, 1.05f, 1.0f, 0.99f, 0.95f, 0.75f, 0.49f, 0.38f, 0.36f };
        static const float onV[] = { 0.67f, 0.86f, 1.27f, 1.0f, 1.0f, 0.95f, 0.66f, 0.45f, 0.37f, 0.20f };
        ColourGradient fg (face.withMultipliedBrightness (on ? onV[0] : upV[0]), 0.0f, r.getY(),
                           face.withMultipliedBrightness (on ? onV[9] : upV[9]), 0.0f, r.getBottom(), false);
        for (int k = 1; k < 9; ++k) fg.addColour (upP[k], face.withMultipliedBrightness (on ? onV[k] : upV[k]));
        g.setGradientFill (fg);
        g.fillRoundedRectangle (r, 1.5f);
        // the side bevels
        for (auto [dx, a] : { std::pair<float, float> { 0.0f, 0.5f }, { 1.0f, 0.3f }, { 2.0f, 0.08f } })
        {
            g.setColour (Colours::black.withAlpha (a));
            g.fillRect (r.getX() + dx, r.getY() + 1.0f, 1.0f, r.getHeight() - 2.0f);
            g.setColour (Colours::black.withAlpha (jmin (0.75f, a * 1.45f)));
            g.fillRect (r.getRight() - 1.0f - dx, r.getY() + 1.0f, 1.0f, r.getHeight() - 2.0f);
        }
        const auto c = r.getCentre().translated (0.0f, -0.5f);
        if (on)
        {
            // yellow, outlined in dark (measured (217, 210, 62))
            g.setColour (Colours::black.withAlpha (0.45f));
            for (auto d : { Point<float> (-0.8f, 0.0f), Point<float> (0.8f, 0.0f), Point<float> (0.0f, -0.8f), Point<float> (0.0f, 0.8f) })
                printText (g, labels[toggle ? 0 : i], c + d, 9.75f, 1.05f, false);
            g.setColour (Colour (240, 232, 64));
        }
        else g.setColour (Colour (66, 66, 64));
        printText (g, labels[toggle ? 0 : i], c, 9.75f, 1.05f, false);
    }
}

void ButtonGroup::mouseDown (const MouseEvent& e)
{
    const int n = toggle ? 1 : labels.size();
    for (int i = 0; i < n; ++i)
        if (buttonRect (i).contains (e.position))
        {
            if (toggle) attachment->setValueAsCompleteGesture (selected != 0 ? 0.0f : 1.0f);
            else attachment->setValueAsCompleteGesture ((float) i);
        }
}

//==============================================================================
// a rounded rectangle with one radius for the top corners and another for the bottom ones
static Path roundRect (Rectangle<float> r, float top, float bottom)
{
    // circular corners: one radius for the top two, another for the bottom two
    Path p;
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom(), hp = MathConstants<float>::halfPi;
    p.startNewSubPath (x0 + top, y0);
    p.lineTo (x1 - top, y0);
    p.addCentredArc (x1 - top, y0 + top, top, top, 0.0f, 0.0f, hp);
    p.lineTo (x1, y1 - bottom);
    p.addCentredArc (x1 - bottom, y1 - bottom, bottom, bottom, 0.0f, hp, 2.0f * hp);
    p.lineTo (x0 + bottom, y1);
    p.addCentredArc (x0 + bottom, y1 - bottom, bottom, bottom, 0.0f, 2.0f * hp, 3.0f * hp);
    p.lineTo (x0, y0 + top);
    p.addCentredArc (x0 + top, y0 + top, top, top, 0.0f, 3.0f * hp, 4.0f * hp);
    p.closeSubPath();
    return p;
}

// a screw sunk in the housing (measured: a recess `recess` px wider than the head, dark on its upper left side, its lower
// right lip catching the light; the head lit from the upper left, with a cross or a slot)
static void screw (Graphics& g, Point<float> c, float r, bool slot, float recess = 1.75f)
{
    const float rr = r + recess;
    g.setColour (Colour (20, 20, 21));
    g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);
    ColourGradient lip (Colours::transparentWhite, c.x - rr * 0.3f, c.y - rr * 0.3f, Colours::white.withAlpha (0.36f), c.x + rr * 0.7f, c.y + rr * 0.7f, false);
    g.setGradientFill (lip);
    g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.3f);
    if (recess > 3.0f)
    {
        // a wide recess: a raised ring around the head
        const float ri = rr - 1.6f;
        g.setColour (Colour (60, 60, 61));
        g.drawEllipse (c.x - ri, c.y - ri, ri * 2.0f, ri * 2.0f, 1.8f);
    }
    ColourGradient sg (slot ? Colour (146, 146, 148) : Colour (57, 57, 58), c.x - r * 0.45f, c.y - r * 0.55f,
                       slot ? Colour (44, 44, 46) : Colour (30, 30, 31), c.x + r * 0.6f, c.y + r * 0.7f, true);
    g.setGradientFill (sg);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
    g.setColour (Colour (20, 20, 21));
    if (slot) g.drawLine (c.x - r * 0.85f, c.y + r * 0.3f, c.x + r * 0.85f, c.y - r * 0.3f, 1.6f);
    else
    {
        g.drawLine (c.x - r * 0.55f, c.y, c.x + r * 0.55f, c.y, 1.8f);
        g.drawLine (c.x, c.y - r * 0.55f, c.x, c.y + r * 0.55f, 1.8f);
    }
}

VuMeter::VuMeter() { setInterceptsMouseClicks (false, false); }

// The printed scale (measured on the reference): an arc of radius 99.8 about (320.7, 220.9) from -57.7 to 60.1 degrees,
// the readings' angles taken at the numbers' centres. The needle turns about a point 0.6, 1.2 px from that centre (its
// line at rest), so its angle is the direction from its pivot to the reading on the arc.
static float scaleAngle (float db)
{
    static const float d[] = { 0.0f, 3.0f, 6.0f, 12.0f, 24.0f, 48.0f }, ang[] = { 59.0f, 24.9f, 0.2f, -29.9f, -49.6f, -58.6f };
    db = jlimit (0.0f, 48.0f, db);
    for (int i = 0; i < 5; ++i)
        if (db <= d[i + 1]) return ang[i] + (ang[i + 1] - ang[i]) * (db - d[i]) / (d[i + 1] - d[i]);
    return ang[5];
}
static constexpr float kArcR = 99.8f, kNeedleDx = 0.6f, kNeedleDy = 1.2f;

float VuMeter::angleForGr (float db)
{
    const float a = rad (scaleAngle (db));
    return radiansToDegrees (std::atan2 (kArcR * std::sin (a) - kNeedleDx, kArcR * std::cos (a) + kNeedleDy));
}

// The dial (measured on the reference, design px; the component covers the black window frame, 197 .. 448 x 73 .. 221,
// and the pivot's bump below it).
// The scale's arc: centre (321.3, 228.5), radius 105, from -55 to 56.5 degrees.
static const Rectangle<float> kCream (7.5f, 7.5f, 236.0f, 135.0f);
static constexpr float kFrameTop = 46.0f, kFrameBottom = 13.0f;      // the window's corner radii (the cream: 7 px less)

static Colour dialColour (float x, float y)                   // design coordinates
{
    // fitted on the reference's dial (print masked out): a warm glow centred high, the shadows of the frame's top and
    // left lips, faint ones along the right and bottom, the glass's reflection at the top left
    auto smooth = [] (float a, float b, float v) { const float t = jlimit (0.0f, 1.0f, (v - a) / (b - a)); return t * t * (3.0f - 2.0f * t); };
    const float k = std::exp (-square ((x - 318.5f) / 80.1f) - square ((y - 55.7f) / 157.3f));
    float r = 185.6f + 53.9f * k, gr = 157.2f + 64.1f * k, b = 99.3f + 67.6f * k;
    float s = 1.0f - 0.752f * (1.0f - smooth (75.0f, 116.0f, y));
    s *= 1.0f - std::exp (-(x - 204.5f) / 6.4f);
    s *= 1.0f - 0.103f * std::exp (-(440.5f - x) / 2.5f) - 0.061f * std::exp (-(215.5f - y) / 2.0f);
    const float refl = jmin (1.0f, 1.018f * std::exp (-square ((x - 274.4f) / 42.25f) - square ((y - 88.0f) / 4.72f)));
    r = r * s; gr = gr * s; b = b * s;
    r += (205.0f - r) * refl; gr += (198.0f - gr) * refl; b += (186.0f - b) * refl;
    return Colour ((uint8) jlimit (0.0f, 255.0f, r), (uint8) jlimit (0.0f, 255.0f, gr), (uint8) jlimit (0.0f, 255.0f, b));
}

void VuMeter::paint (Graphics& g)
{
    const auto o = getPosition().toFloat();                      // design origin of the component
    const auto cream = roundRect (kCream, kFrameTop - 7.0f, kFrameBottom - 7.0f);
    const float scale = jlimit (0.5f, 4.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 4.0f) / 4.0f);
    if (dial.isNull() || dialScale != scale)
    {
        dialScale = scale;
        const int w = (int) std::ceil (kCream.getWidth() * scale), h = (int) std::ceil (kCream.getHeight() * scale);
        dial = Image (Image::RGB, w, h, false);
        Image::BitmapData bd (dial, Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                bd.setPixelColour (x, y, dialColour (o.x + kCream.getX() + ((float) x + 0.5f) / scale, o.y + kCream.getY() + ((float) y + 0.5f) / scale));
    }
    g.saveState();
    g.reduceClipRegion (cream);
    g.drawImageTransformed (dial, AffineTransform::scale (1.0f / scale).translated (kCream.getX(), kCream.getY()));

    const auto c = pivot;
    const float R = kArcR;
    auto at = [&] (float deg, float r) { return c + Point<float> (std::sin (rad (deg)), -std::cos (rad (deg))) * r; };
    const Colour ink (34, 30, 24);
    // the red band from 6 to 0 outside the arc (to radius 107), its dividers at the dots
    {
        Path band;
        band.addCentredArc (c.x, c.y, 107.0f, 107.0f, 0.0f, rad (0.2f), rad (60.0f), true);
        band.addCentredArc (c.x, c.y, R, R, 0.0f, rad (60.0f), rad (0.2f), false);
        band.closeSubPath();
        g.setColour (Colour (234, 85, 66));
        g.fillPath (band);
        g.setColour (ink);
        g.strokePath (band, PathStrokeType (1.1f));
        for (float a : { 7.7f, 15.6f, 24.6f, 35.6f, 46.5f })
            g.drawLine (Line<float> (at (a, R), at (a, 107.0f)), 1.0f);
    }
    // the arc, its ticks standing outward on it (minor 2.3 px, at 12 and 24 6 px), the stop at the 48 end
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, R, R, 0.0f, rad (-57.7f), rad (60.1f), true);
        g.setColour (ink);
        g.strokePath (arc, PathStrokeType (1.5f));
        g.drawLine (Line<float> (at (-57.7f, R - 0.7f), at (-57.7f, 107.9f)), 1.4f);
        for (float a : { -51.8f, -46.4f, -40.5f, -34.5f, -28.9f, -21.6f, -14.7f, -7.9f })
            g.drawLine (Line<float> (at (a, R), at (a, a == -51.8f || a == -28.9f ? 107.9f : 103.8f)), 1.0f);
    }
    for (float a : { -39.3f, -19.6f, -10.0f, 7.8f, 15.7f, 34.7f, 46.9f })
    {
        const auto p = at (a, 117.2f);
        g.drawEllipse (p.x - 2.3f, p.y - 2.3f, 4.6f, 4.6f, 1.0f);
    }
    // the numbers: Times bold italic, each turned along the scale (measured centres)
    {
        const Font f = Font (FontOptions ("Times New Roman", 10.0f, Font::bold | Font::italic).withPointHeight (14.5f));
        struct Num { float a, r; const char* t; };
        for (auto n : { Num { -58.6f, 116.3f, "48" }, Num { -49.6f, 116.4f, "24" }, Num { -29.9f, 116.0f, "12" }, Num { 0.2f, 115.5f, "6" },
                        Num { 24.9f, 116.8f, "3" }, Num { 59.0f, 117.1f, "0" } })
        {
            GlyphArrangement ga;
            ga.addLineOfText (f, n.t, 0.0f, 0.0f);
            Path p;
            ga.createPath (p);
            const auto b = p.getBounds();
            g.fillPath (p, AffineTransform::translation (-b.getCentreX(), -b.getCentreY()).rotated (rad (n.a)).translated (at (n.a, n.r)));
        }
    }
    // our mark printed on the dial where the reference prints its maker's logo
    drawMark (g, Rectangle<float> (c.x - 18.4f, c.y - 69.1f, 38.0f, 29.0f), false, false, Colour (96, 78, 50).withAlpha (0.8f), 1.2f);
    // the needle (measured: (124, 0, 0), 2 px, to 114 px from its pivot, a cream rivet at 109 px) and its soft shadow,
    // 5 px below it, 3.5 px wide, a quarter darker
    {
        const Point<float> np (c.x + kNeedleDx, c.y + kNeedleDy), dir (std::sin (rad (needle)), -std::cos (rad (needle))), nrm (-dir.y, dir.x);
        const Point<float> base = np + dir * 20.0f, tip = np + dir * 114.0f;
        const Point<float> sh = nrm * 5.0f;
        g.setColour (Colour (70, 52, 24).withAlpha (0.10f));
        g.drawLine (Line<float> (base + sh, tip + sh - dir * 1.5f), 5.0f);
        g.setColour (Colour (70, 52, 24).withAlpha (0.20f));
        g.drawLine (Line<float> (base + sh, tip + sh - dir * 2.0f), 2.6f);
        g.setColour (kNeedle);
        g.drawLine (Line<float> (base, tip), 1.7f);
        const auto rv = np + dir * 109.0f;
        g.fillEllipse (rv.x - 2.0f, rv.y - 2.0f, 4.0f, 4.0f);
        g.setColour (Colour (255, 250, 168));
        g.fillEllipse (rv.x - 0.9f, rv.y - 0.9f, 1.8f, 1.8f);
    }
    g.restoreState();

    // the housing's bump into the window over the pivot (measured: housing grey, edged by the black recess), with the
    // slotted pivot screw in its recess
    const Point<float> bc (c.x + 0.4f, c.y - 0.9f);
    g.saveState();
    g.reduceClipRegion (Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 147.0f).toNearestInt());     // the recess edge only in the window
    g.setColour (Colours::black);
    g.fillEllipse (bc.x - 21.3f, bc.y - 21.3f, 42.6f, 42.6f);
    g.restoreState();
    g.setColour (Colour (43, 43, 44));
    g.fillEllipse (bc.x - 18.3f, bc.y - 18.3f, 36.6f, 36.6f);
    screw (g, { c.x + 0.5f, c.y - 2.15f }, 8.3f, true, 3.9f);
}

void drawVuHousing (Graphics& g, Rectangle<float> h, Point<float> archCentre, float archRadius)
{
    // the housing's outline: a rounded rectangle whose bottom edge arches up around the Threshold knob, the arch
    // joining the edge in small round fillets (measured)
    const float R = 12.0f, F = 6.0f, x0 = h.getX(), y0 = h.getY(), x1 = h.getRight(), y1 = h.getBottom(), hp = MathConstants<float>::halfPi;
    const auto C = archCentre;
    const float fy = y1 - F, fdx = std::sqrt (jmax (0.0f, square (archRadius + F) - square (fy - C.y)));
    const Point<float> fR (C.x + fdx, fy), fL (C.x - fdx, fy);                  // the fillets' centres
    auto towards = [] (Point<float> from, Point<float> to) { return std::atan2 (to.x - from.x, -(to.y - from.y)); };
    Path p;
    p.startNewSubPath (x0 + R, y0);
    p.lineTo (x1 - R, y0);
    p.addCentredArc (x1 - R, y0 + R, R, R, 0.0f, 0.0f, hp);
    p.lineTo (x1, y1 - R);
    p.addCentredArc (x1 - R, y1 - R, R, R, 0.0f, hp, 2.0f * hp);
    p.lineTo (fR.x, y1);
    p.addCentredArc (fR.x, fR.y, F, F, 0.0f, 2.0f * hp, towards (fR, C) + MathConstants<float>::twoPi);
    p.addCentredArc (C.x, C.y, archRadius, archRadius, 0.0f, towards (C, fR), towards (C, fL));
    p.addCentredArc (fL.x, fL.y, F, F, 0.0f, towards (fL, C), 2.0f * hp);
    p.lineTo (x0 + R, y1);
    p.addCentredArc (x0 + R, y1 - R, R, R, 0.0f, 2.0f * hp, 3.0f * hp);
    p.lineTo (x0, y0 + R);
    p.addCentredArc (x0 + R, y0 + R, R, R, 0.0f, 3.0f * hp, 4.0f * hp);
    p.closeSubPath();

    // inside the arch, the panel lies in the housing's shadow (measured: from 6.9 px inside the edge, about 0.7 soon after,
    // 0.4 at the edge)
    {
        g.saveState();
        g.reduceClipRegion (Rectangle<float> (x0, y0, h.getWidth(), y1 - y0).toNearestInt());
        ColourGradient sh (Colours::transparentBlack, C.x, C.y, Colours::black.withAlpha (0.62f), C.x + archRadius, C.y, true);
        sh.addColour ((archRadius - 6.9f) / archRadius, Colours::transparentBlack);
        sh.addColour ((archRadius - 5.3f) / archRadius, Colours::black.withAlpha (0.30f));
        sh.addColour ((archRadius - 3.0f) / archRadius, Colours::black.withAlpha (0.38f));
        sh.addColour ((archRadius - 1.6f) / archRadius, Colours::black.withAlpha (0.46f));
        g.setGradientFill (sh);
        g.fillEllipse (C.x - archRadius, C.y - archRadius, archRadius * 2.0f, archRadius * 2.0f);
        g.restoreState();
    }

    // its shadow on the panel (measured: about 4 px below and to the right)
    for (int k = 5; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.08f));
        g.fillPath (p, AffineTransform::translation (0.7f * (float) k, 0.8f * (float) k));
    }
    // a raised box lit from the upper left (measured): the top face a 5 px light band, the left face a thin light line,
    // the right face a 6 px dark band, the bottom face and the arch's edge 5 px dark bands; the top a flat (43, 43, 43)
    // that lightens toward the bottom edge
    g.saveState();
    g.reduceClipRegion (p);
    g.setColour (Colour (25, 25, 26));
    g.fillPath (p);
    g.setColour (Colour (98, 98, 99));
    g.fillPath (p, AffineTransform::translation (-6.0f, -5.0f));
    g.reduceClipRegion (p, AffineTransform::translation (-6.0f, -5.0f));
    ColourGradient hg (Colour (43, 43, 43), 0.0f, y1 - 30.0f, Colour (52, 52, 52), 0.0f, y1 - 6.0f, false);
    g.setGradientFill (hg);
    g.fillPath (p, AffineTransform::translation (2.5f, 6.0f));
    g.restoreState();
    // the arch's edge faces down toward the knob: an even 5.6 px dark lip all round it (measured), outlined. Around it
    // the top face again, above the bottom band: the box's offset bevel layers leave slivers along a concave edge.
    {
        const float lw = 5.6f, rl = archRadius + lw;
        g.saveState();
        g.reduceClipRegion (p);
        {
            g.saveState();
            Path near;
            near.addEllipse (C.x - rl - 6.0f, C.y - rl - 6.0f, (rl + 6.0f) * 2.0f, (rl + 6.0f) * 2.0f);
            g.reduceClipRegion (near);
            g.reduceClipRegion (Rectangle<float> (x0, y0, h.getWidth(), y1 - 5.0f - y0).toNearestInt());
            g.setGradientFill (hg);
            g.fillPath (p);
            g.restoreState();
        }
        Path lip;
        lip.addEllipse (C.x - rl, C.y - rl, rl * 2.0f, rl * 2.0f);
        g.reduceClipRegion (lip);
        g.setColour (Colour (26, 26, 27));
        g.fillPath (p);
        g.setColour (Colour (17, 17, 17));
        g.drawEllipse (C.x - rl + 0.5f, C.y - rl + 0.5f, rl * 2.0f - 1.0f, rl * 2.0f - 1.0f, 1.2f);
        g.restoreState();
    }
    g.setColour (Colour (30, 30, 30));
    g.strokePath (p, PathStrokeType (1.2f));

    // the dial's window: a black recess, its lower lip catching the light
    const auto frame = roundRect ({ 197.5f, 73.5f, 250.0f, 147.0f }, kFrameTop, kFrameBottom);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.strokePath (frame, PathStrokeType (1.0f), AffineTransform::translation (0.0f, 1.0f));
    g.setColour (Colours::black);
    g.fillPath (frame);
    for (auto s : { Point<float> (198.4f, 75.95f), Point<float> (196.6f, 261.05f), Point<float> (449.2f, 260.95f) })
        screw (g, s, 9.0f, false, 2.2f);
}

// one character of the display's segment font in the cell r (segments a..g as a 7-segment digit, i: the upper middle
// stroke of M), thickness t, leaning right by `slant` px per px of height (measured on the reference's display)
static void segChar (Graphics& g, Rectangle<float> r, juce_wchar ch, float t, float slant)
{
    enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64, I = 128 };
    int m = 0;
    switch (CharacterFunctions::toUpperCase (ch))
    {
        case '0': case 'O': m = A | B | C | D | E | F; break;
        case '1': m = B | C; break;
        case '2': m = A | B | G | E | D; break;
        case '3': m = A | B | G | C | D; break;
        case '4': m = F | G | B | C; break;
        case '5': case 'S': m = A | F | G | C | D; break;
        case '6': m = A | F | G | E | C | D; break;
        case '7': m = A | B | C; break;
        case '8': case 'B': m = A | B | C | D | E | F | G; break;
        case '9': m = A | B | C | D | F | G; break;
        case 'P': m = A | B | F | G | E; break;
        case 'M': m = A | B | C | E | F | I; break;
        case 'F': m = A | F | G | E; break;
        case 'H': m = B | C | E | F | G; break;
        case 'T': m = A | I; break;
        case '-': m = G; break;
        default: break;
    }
    const float w = r.getWidth(), h = r.getHeight(), x0 = r.getX(), y0 = r.getY(), hm = std::round (h * 0.5f - t * 0.5f), gap = t * 0.35f;
    auto rect = [&] (float x, float y, float rw, float rh)
    {
        // a segment, sheared by the slant
        Path p;
        p.addRectangle (x0 + x, y0 + y, rw, rh);
        g.fillPath (p, AffineTransform::shear (-slant, 0.0f).translated (slant * (y0 + h), 0.0f));
    };
    if (m & A) rect (gap, 0.0f, w - 2.0f * gap, t);
    if (m & D) rect (gap, h - t, w - 2.0f * gap, t);
    if (m & G) rect (gap, hm, w - 2.0f * gap, t);
    if (m & F) rect (0.0f, gap, t, hm - gap);
    if (m & E) rect (0.0f, hm + t, t, h - hm - t - gap);
    if (m & B) rect (w - t, gap, t, hm - gap);
    if (m & C) rect (w - t, hm + t, t, h - hm - t - gap);
    if (m & I) rect (std::round (w * 0.5f - t * 0.5f), gap, t, hm - gap);
}

void drawLedDisplay (Graphics& g, Rectangle<float> box, const String& text, const String& digits)
{
    // measured: a black-rimmed (19, 19, 19) window; 6 x 9 px characters at an 8 px pitch from 7 px in, 9 px from the top;
    // the tempo in 12 x 20 px digits at a 16 px pitch, right-aligned 8.5 px from the right edge, 7 px from the top
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (box.translated (0.5f * (float) k, 1.0f * (float) k), 5.0f);
    }
    g.setColour (Colours::black);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (Colour (19, 19, 19));
    g.fillRoundedRectangle (box.reduced (2.0f), 3.5f);
    g.setColour (Colour (236, 142, 22));
    for (int i = 0; i < text.length(); ++i)
        segChar (g, { box.getX() + 7.0f + 8.0f * (float) i, box.getY() + 13.0f, 6.0f, 9.0f }, text[i], 1.0f, 0.11f);
    for (int i = 0; i < digits.length(); ++i)
        segChar (g, { box.getRight() - 8.5f - 12.0f - 16.0f * (float) (digits.length() - 1 - i), box.getY() + 7.0f, 12.0f, 20.0f },
                 digits[i], 2.0f, 0.1f);
}

void drawLogo (Graphics& g, Point<float> leftCentre, const String& first, const String& rest)
{
    // measured: the first letter Arial Black 32 px, the rest Arial 30.5 px, 2.7 px apart, inks centred on one line
    auto ink = [] (const Font& f, const String& t, Path& p)
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        ga.createPath (p);
        return p.getBounds();
    };
    Path p1, p2;
    const auto b1 = ink (Font (FontOptions ("Arial Black", 10.0f, Font::plain).withPointHeight (32.0f)), first, p1);
    const auto b2 = ink (Font (FontOptions ("Arial", 10.0f, Font::plain).withPointHeight (30.5f)), rest, p2);
    g.setColour (Colour (22, 14, 8));
    g.fillPath (p1, AffineTransform::translation (leftCentre.x - b1.getX(), leftCentre.y - b1.getCentreY()));
    g.fillPath (p2, AffineTransform::translation (leftCentre.x + b1.getWidth() + 2.7f - b2.getX(), leftCentre.y - b2.getCentreY()));
}

//==============================================================================
Slider::SliderLayout HybridLook::getSliderLayout (Slider& s)
{
    if (auto* k = dynamic_cast<Knob*> (&s))
    {
        Slider::SliderLayout l;
        l.sliderBounds = s.getLocalBounds();
        l.textBoxBounds = k->valueArea.isEmpty() ? Rectangle<int> (0, 0, 1, 1) : k->valueArea;
        return l;
    }
    return FlatLook::getSliderLayout (s);
}

Label* HybridLook::createSliderTextBox (Slider& s)
{
    auto* l = FlatLook::createSliderTextBox (s);
    if (dynamic_cast<Knob*> (&s) != nullptr) l->setColour (Label::textColourId, Colours::transparentBlack);   // shown only while typing
    return l;
}
}
