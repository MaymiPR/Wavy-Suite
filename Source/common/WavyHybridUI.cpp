#include "WavyHybridUI.h"
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

static void shadow (Graphics& g, Point<float> c, float r, float dx, float dy)
{
    for (int k = 6; k >= 1; --k)
    {
        const float rr = r + (float) k * 1.2f;
        g.setColour (Colour (40, 16, 4).withAlpha (0.07f));
        g.fillEllipse (c.x + dx - rr, c.y + dy - rr, rr * 2.0f, rr * 2.0f);
    }
}

static Path fluted (Point<float> c, float r, int lobes, float depth)
{
    Path p;
    const int n = 360;
    for (int i = 0; i <= n; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / (float) n;
        const float s = std::abs (std::cos (a * (float) lobes * 0.5f));
        const float rr = r - depth * (1.0f - std::pow (s, 0.35f));
        const Point<float> q (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    p.closeSubPath();
    return p;
}

static void bakelite (Graphics& g, const Path& body, Point<float> c, float r)
{
    ColourGradient bg (Colour (78, 78, 80), c.x - r * 0.55f, c.y - r * 0.65f, Colour (10, 10, 11), c.x + r * 0.6f, c.y + r * 0.75f, true);
    g.setGradientFill (bg);
    g.fillPath (body);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (body, PathStrokeType (1.0f));
}

static void cap (Graphics& g, Point<float> c, float r)
{
    ColourGradient cg (Colour (58, 58, 60), c.x - r * 0.5f, c.y - r * 0.6f, Colour (16, 16, 17), c.x + r * 0.5f, c.y + r * 0.7f, false);
    g.setGradientFill (cg);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colour (110, 110, 112).withAlpha (0.55f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.9f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - r - 1.0f, c.y - r - 1.0f, r * 2.0f + 2.0f, r * 2.0f + 2.0f, 0.8f);
    // soft specular
    ColourGradient sp (Colours::white.withAlpha (0.16f), c.x - r * 0.35f, c.y - r * 0.45f, Colours::transparentWhite, c.x - r * 0.35f + r * 0.6f, c.y - r * 0.45f + r * 0.6f, true);
    g.setGradientFill (sp);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
}

static void pointer (Graphics& g, Point<float> c, float a, float r0, float r1, float w)
{
    const Point<float> d (std::sin (a), -std::cos (a)), n (-d.y, d.x);
    Path p;
    p.startNewSubPath (c + d * r0 - n * (w * 0.5f));
    p.lineTo (c + d * r1 - n * (w * 0.18f));
    p.lineTo (c + d * (r1 + 1.5f));
    p.lineTo (c + d * r1 + n * (w * 0.18f));
    p.lineTo (c + d * r0 + n * (w * 0.5f));
    p.closeSubPath();
    g.setColour (Colour (40, 30, 15).withAlpha (0.6f));
    g.strokePath (p, PathStrokeType (1.0f));
    ColourGradient pg (Colour (246, 238, 205), c.x + n.x * w, c.y + n.y * w, Colour (176, 160, 118), c.x - n.x * w, c.y - n.y * w, false);
    g.setGradientFill (pg);
    g.fillPath (p);
}

void Knob::paint (Graphics& g)
{
    const auto c = centre;
    const float a = pointerAngle();
    if (kind == Small)
    {
        const float r = 33.0f;
        shadow (g, c, r, 10.0f, 14.0f);
        bakelite (g, fluted (c, r, 16, 1.2f), c, r);
        cap (g, c, r * 0.70f);
        pointer (g, c, a, r - 5.0f, r + 8.0f, 5.0f);
    }
    else
    {
    const float rs = radius(), rb = kind == Large ? 46.0f : 63.0f, rc = kind == Large ? 33.0f : 45.0f;
    shadow (g, c, rs, 10.0f, 13.0f);
    // satin chrome skirt
    ColourGradient sk (Colour (232, 229, 218), c.x - rs * 0.6f, c.y - rs * 0.7f, Colour (92, 90, 84), c.x + rs * 0.7f, c.y + rs * 0.8f, false);
    sk.addColour (0.45, Colour (178, 175, 164));
    g.setGradientFill (sk);
    g.fillEllipse (c.x - rs, c.y - rs, rs * 2.0f, rs * 2.0f);
    g.setColour (Colour (50, 48, 44).withAlpha (0.7f));
    g.drawEllipse (c.x - rs, c.y - rs, rs * 2.0f, rs * 2.0f, 1.0f);
    pointer (g, c, a, rb - 8.0f, rs + 1.0f, 4.5f);
    // the fluted black knob over it
    const auto body = fluted (c, rb, 12, kind == Large ? 2.4f : 3.2f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillPath (body, AffineTransform::translation (2.0f, 3.0f));
    bakelite (g, body, c, rb);
    cap (g, c, rc);
    }
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

Rectangle<float> ButtonGroup::buttonRect (int i) const
{
    const auto b = getLocalBounds().toFloat().reduced (3.0f);
    const int n = toggle ? 1 : labels.size();
    if (vertical)
    {
        const float h = (b.getHeight() - 2.0f * (float) (n - 1)) / (float) n;
        return { b.getX(), b.getY() + (float) i * (h + 2.0f), b.getWidth(), h };
    }
    const float w = (b.getWidth() - 2.0f * (float) (n - 1)) / (float) n;
    return { b.getX() + (float) i * (w + 2.0f), b.getY(), w, b.getHeight() };
}

void ButtonGroup::paint (Graphics& g)
{
    const auto f = getLocalBounds().toFloat();
    g.setColour (Colour (18, 16, 14));
    g.fillRoundedRectangle (f, 4.0f);
    const int n = toggle ? 1 : labels.size();
    for (int i = 0; i < n; ++i)
    {
        const auto r = buttonRect (i);
        const bool on = toggle ? selected != 0 : selected == i;
        ColourGradient bg (on ? Colour (152, 148, 116) : Colour (238, 236, 224), 0.0f, r.getY(),
                           on ? Colour (112, 108, 82) : Colour (190, 187, 172), 0.0f, r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colours::white.withAlpha (on ? 0.12f : 0.55f));
        g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
        g.setColour (Colour (70, 66, 56));
        g.drawRoundedRectangle (r, 3.0f, 0.8f);
        g.setColour (on ? kSelText : Colour (58, 55, 48));
        g.setFont (font (r.getHeight() > 16.0f ? 12.0f : 11.0f));
        g.drawText (labels[toggle ? 0 : i], r, Justification::centred);
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
static void screw (Graphics& g, Point<float> c, float r, bool slot)
{
    ColourGradient sg (Colour (92, 92, 94), c.x - r * 0.5f, c.y - r * 0.6f, Colour (26, 26, 28), c.x + r * 0.6f, c.y + r * 0.7f, false);
    g.setGradientFill (sg);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    g.setColour (Colour (12, 12, 12));
    if (slot) g.drawLine (c.x - r * 0.75f, c.y + r * 0.2f, c.x + r * 0.75f, c.y - r * 0.2f, 2.2f);
    else
    {
        g.drawLine (c.x - r * 0.55f, c.y, c.x + r * 0.55f, c.y, 1.8f);
        g.drawLine (c.x, c.y - r * 0.55f, c.x, c.y + r * 0.55f, 1.8f);
    }
}

VuMeter::VuMeter() { setInterceptsMouseClicks (false, false); }

// the dial's printed scale (degrees from vertical for a reading in dB of gain reduction), measured on the dial
static float vuAngle (float db)
{
    static const float d[] = { 0.0f, 3.0f, 6.0f, 12.0f, 24.0f, 48.0f }, ang[] = { 56.0f, 23.0f, 0.0f, -29.0f, -49.0f, -56.0f };
    db = jlimit (0.0f, 48.0f, db);
    for (int i = 0; i < 5; ++i)
        if (db <= d[i + 1]) return ang[i] + (ang[i + 1] - ang[i]) * (db - d[i]) / (d[i + 1] - d[i]);
    return ang[5];
}

static void wavyMark (Graphics& g, Rectangle<float> r, Colour c)
{
    // three stepped sine lobes, as the Wavy mark, in one colour
    Path p;
    const float w = r.getWidth() / 3.0f;
    for (int k = 0; k < 3; ++k)
    {
        const float x0 = r.getX() + (float) k * w;
        const float up = k == 1 ? -1.0f : 1.0f;
        p.startNewSubPath (x0, r.getCentreY());
        for (int i = 0; i <= 20; ++i)
        {
            const float t = (float) i / 20.0f;
            p.lineTo (x0 + t * w, r.getCentreY() - up * std::sin (t * MathConstants<float>::pi) * r.getHeight() * 0.5f);
        }
        p.closeSubPath();
    }
    g.setColour (c);
    g.fillPath (p);
}

void VuMeter::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Path win;
    win.addRoundedRectangle (b, 18.0f);
    ColourGradient dg (kDialLight, b.getCentreX(), b.getY() + 10.0f, kDialDark, b.getX(), b.getBottom(), true);
    g.setGradientFill (dg);
    g.fillPath (win);
    // brushed texture
    Random rnd (77);
    g.saveState();
    g.reduceClipRegion (win);
    for (int i = 0; i < 90; ++i)
    {
        const float y = b.getY() + rnd.nextFloat() * b.getHeight();
        g.setColour ((rnd.nextBool() ? Colours::white : Colour (120, 96, 50)).withAlpha (0.05f));
        g.drawLine (b.getX(), y, b.getRight(), y + rnd.nextFloat() * 6.0f - 3.0f, 0.8f);
    }
    // scale
    const auto c = pivot;
    const float R = 102.0f;
    auto at = [&] (float deg, float r) { return c + Point<float> (std::sin (rad (deg)), -std::cos (rad (deg))) * r; };
    Path red;
    red.addCentredArc (c.x, c.y, R + 3.5f, R + 3.5f, 0.0f, rad (vuAngle (6.0f)), rad (vuAngle (0.0f)), true);
    g.setColour (Colour (226, 74, 56));
    g.strokePath (red, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::butt));
    g.setColour (Colour (120, 24, 18));
    for (float db : { 0.5f, 1.2f, 2.0f, 3.0f, 4.2f })
        g.drawLine (Line<float> (at (vuAngle (db), R), at (vuAngle (db), R + 7.0f)), 1.0f);
    Path arc;
    arc.addCentredArc (c.x, c.y, R, R, 0.0f, rad (-56.0f), rad (56.0f), true);
    g.setColour (Colour (22, 18, 12));
    g.strokePath (arc, PathStrokeType (1.8f));
    for (float db : { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 20.0f, 24.0f, 32.0f, 40.0f, 48.0f })
    {
        const bool major = db == 0.0f || db == 3.0f || db == 6.0f || db == 12.0f || db == 24.0f || db == 48.0f;
        if (db >= 6.0f) g.drawLine (Line<float> (at (vuAngle (db), R - (major ? 7.0f : 5.0f)), at (vuAngle (db), R)), 1.2f);
    }
    g.setFont (Font (FontOptions ("Times New Roman", 17.0f, Font::bold | Font::italic)));
    for (auto [db, t] : { std::pair<float, const char*> { 48.0f, "48" }, { 24.0f, "24" }, { 12.0f, "12" }, { 6.0f, "6" }, { 3.0f, "3" }, { 0.0f, "0" } })
    {
        const auto p = at (vuAngle (db) + (db == 48.0f ? -4.0f : db == 0.0f ? 3.0f : 0.0f), R + 18.0f);
        g.drawText (t, Rectangle<float> (p.x - 14.0f, p.y - 9.0f, 28.0f, 18.0f), Justification::centred);
    }
    for (float db : { 1.0f, 2.0f, 4.0f, 5.0f, 8.0f, 10.0f, 16.0f })
    {
        const auto p = at (vuAngle (db), R + 15.0f);
        g.drawEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f, 1.1f);
    }
    wavyMark (g, Rectangle<float> (c.x - 19.0f, c.y - 72.0f, 38.0f, 26.0f), Colour (96, 78, 50).withAlpha (0.75f));
    // needle
    const Point<float> tip = at (needle, R + 8.0f), base = at (needle, 22.0f);
    g.setColour (Colours::black.withAlpha (0.22f));
    g.drawLine (Line<float> (base + Point<float> (2.0f, 2.5f), tip + Point<float> (2.0f, 2.5f)), 1.6f);
    g.setColour (kNeedle);
    g.drawLine (Line<float> (base, tip), 1.6f);
    // glass glare along the top
    ColourGradient gl (Colours::white.withAlpha (0.30f), 0.0f, b.getY(), Colours::transparentWhite, 0.0f, b.getY() + 34.0f, false);
    g.setGradientFill (gl);
    g.fillRect (b.withHeight (36.0f));
    g.restoreState();
    // inner shadow of the window edge
    g.setColour (Colours::black.withAlpha (0.55f));
    g.strokePath (win, PathStrokeType (2.0f));
    // the housing's bump over the lower edge, with the pivot screw
    const Point<float> bc (c.x, b.getBottom() + 4.0f);
    g.setColour (Colour (30, 30, 31));
    g.fillEllipse (bc.x - 27.0f, bc.y - 27.0f, 54.0f, 54.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (bc.x - 27.0f, bc.y - 27.0f, 54.0f, 54.0f, 1.0f);
    screw (g, { bc.x, bc.y - 6.0f }, 10.0f, true);
}

void drawVuHousing (Graphics& g, Rectangle<float> h, Point<float> archCentre, float archRadius)
{
    Path p;
    p.addRoundedRectangle (h, 14.0f);
    Path cut;
    cut.addEllipse (archCentre.x - archRadius, archCentre.y - archRadius, archRadius * 2.0f, archRadius * 2.0f);
    p.setUsingNonZeroWinding (false);
    p.addPath (cut);
    for (int k = 8; k >= 1; --k)
    {
        g.setColour (Colour (40, 16, 4).withAlpha (0.06f));
        g.fillPath (p, AffineTransform::translation (2.0f + (float) k * 0.9f, 3.0f + (float) k * 1.1f));
    }
    ColourGradient hg (Colour (46, 46, 47), h.getX(), h.getY(), Colour (20, 20, 21), h.getX(), h.getBottom(), false);
    g.setGradientFill (hg);
    g.fillPath (p);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (p, PathStrokeType (1.2f));
    // the dial's recessed bezel
    const Rectangle<float> bez (h.getX() + 22.0f, h.getY() + 25.0f, h.getWidth() - 44.0f, 145.0f);
    g.setColour (Colour (8, 8, 8));
    g.fillRoundedRectangle (bez, 21.0f);
    g.setColour (Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (bez.expanded (1.0f), 22.0f, 1.0f);
    for (auto s : { Point<float> (h.getX() + 22.0f, h.getY() + 23.0f), Point<float> (h.getX() + 22.0f, h.getBottom() - 23.0f),
                    Point<float> (h.getRight() - 22.0f, h.getBottom() - 23.0f) })
        screw (g, s, 8.5f, false);
}

// one 7-segment digit, slanted like the reference's display (segments a..g)
static void segDigit (Graphics& g, Rectangle<float> r, int d, float slant)
{
    static const int mask[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
    if (d < 0 || d > 9) { if (d == -1) { const float y = r.getCentreY(); g.fillRect (r.getX() + 2.0f, y - 1.0f, r.getWidth() - 4.0f, 2.2f); } return; }
    const int m = mask[d];
    const float w = r.getWidth(), h = r.getHeight(), t = 2.8f, x0 = r.getX(), y0 = r.getY();
    auto sk = [&] (float x, float y) { return Point<float> (x0 + x + slant * (h - y), y0 + y); };
    auto seg = [&] (Point<float> a, Point<float> b)
    {
        Path p; p.startNewSubPath (a); p.lineTo (b);
        g.strokePath (p, PathStrokeType (t, PathStrokeType::mitered, PathStrokeType::butt));
    };
    const float hm = h * 0.5f, gap = 1.2f;
    if (m & 0x01) seg (sk (gap + t * 0.5f, t * 0.5f), sk (w - gap - t * 0.5f, t * 0.5f));                 // a
    if (m & 0x02) seg (sk (w - t * 0.5f, gap + t * 0.5f), sk (w - t * 0.5f, hm - gap));                   // b
    if (m & 0x04) seg (sk (w - t * 0.5f, hm + gap), sk (w - t * 0.5f, h - gap - t * 0.5f));              // c
    if (m & 0x08) seg (sk (gap + t * 0.5f, h - t * 0.5f), sk (w - gap - t * 0.5f, h - t * 0.5f));         // d
    if (m & 0x10) seg (sk (t * 0.5f, hm + gap), sk (t * 0.5f, h - gap - t * 0.5f));                       // e
    if (m & 0x20) seg (sk (t * 0.5f, gap + t * 0.5f), sk (t * 0.5f, hm - gap));                           // f
    if (m & 0x40) seg (sk (gap + t * 0.5f, hm), sk (w - gap - t * 0.5f, hm));                             // g
}

void drawLedDisplay (Graphics& g, Rectangle<float> box, const String& text, const String& digits)
{
    g.setColour (Colour (12, 10, 9));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (Colour (66, 58, 50));
    g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
    if (digits.isNotEmpty())
    {
        // "BPM 120": a small caption, then large slanted digits right-aligned (measured on the reference)
        g.setColour (kAmber);
        g.setFont (Font (FontOptions ("Consolas", 10.5f, Font::italic)));
        g.drawText (text, box.withTrimmedLeft (7.0f).withWidth (30.0f), Justification::centredLeft);
        const float dw = 15.0f, dh = 23.0f, gap = 4.0f;
        float x = box.getRight() - 9.0f - (float) digits.length() * (dw + gap) + gap;
        for (auto ch : digits)
        {
            segDigit (g, { x, box.getCentreY() - dh * 0.5f, dw, dh }, ch == '-' ? -1 : (int) (ch - '0'), 0.12f);
            x += dw + gap;
        }
        return;
    }
    const auto t = box.reduced (10.0f, 4.0f);
    const Font f (FontOptions ("Consolas", 12.0f, Font::italic));
    g.setFont (f);
    g.setColour (kAmber.withAlpha (0.25f));
    for (auto d : { Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (text, t.translated (d.x, d.y), Justification::centredLeft);
    g.setColour (kAmber);
    g.drawText (text, t, Justification::centredLeft);
}

void drawLogo (Graphics& g, Rectangle<float> area, const String& first, const String& rest)
{
    const Font bold (FontOptions ("Arial", area.getHeight(), Font::bold)), light (FontOptions ("Arial", area.getHeight() * 0.94f, Font::plain));
    const float w1 = GlyphArrangement::getStringWidth (bold, first);
    g.setColour (Colour (22, 14, 8));
    g.setFont (bold);
    g.drawText (first, area.withWidth (w1 + 2.0f), Justification::centredLeft);
    g.setFont (light);
    g.drawText (rest, area.withTrimmedLeft (w1 + 1.0f), Justification::centredLeft);
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
