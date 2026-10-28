#include "WVoxChainEditor.h"
#include "WVoxChainSkinData.h"
#include "WavySkinModels.h"
#include "WavyLogo.h"

using namespace juce;
namespace sk = wvoxchain::skin;

namespace wvoxchain::ui
{
namespace
{
float physicalScale (Graphics& g)
{
    return jlimit (0.25f, 8.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 100.0f) / 100.0f);
}

template <typename Make>
const Image& sprite (Sprite& sp, float s, Make make)
{
    if (! sp.image.isValid() || std::abs (sp.scale - s) > 1.0e-3f) { sp.image = make (s); sp.scale = s; }
    return sp.image;
}

// a sprite rendered at physical scale s, its top left at (x, y) design px
void drawSprite (Graphics& g, const Image& img, float s, float x, float y)
{
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::scale (1.0f / s));
    g.setOpacity (1.0f);
    g.drawImageAt (img, roundToInt (x * s), roundToInt (y * s));
}

Font font (float h, bool bold, bool italic)
{
    return Font (FontOptions ("Arial", h, (bold ? Font::bold : 0) | (italic ? Font::italic : 0)));
}

// a string's outline fitted into an ink box (the font's ink stretched to it)
Path fittedText (const String& text, Rectangle<float> box, bool bold, bool italic)
{
    GlyphArrangement ga;
    ga.addLineOfText (font (24.0f, bold, italic), text, 0.0f, 0.0f);
    Path p;
    ga.createPath (p);
    const auto b = p.getBounds();
    if (! b.isEmpty())
        p.applyTransform (AffineTransform::fromTargetPoints (b.getX(), b.getY(), box.getX(), box.getY(),
                                                              b.getRight(), b.getY(), box.getRight(), box.getY(),
                                                              b.getX(), b.getBottom(), box.getX(), box.getBottom()));
    return p;
}

// a printed label: the font's outline fitted into the ink box measured on the reference
void drawLabel (Graphics& g, const sk::Label& l)
{
    g.setColour (Colour::greyLevel (l.grey / 255.0f));
    g.fillPath (fittedText (String::fromUTF8 (l.text), Rectangle<float>::leftTopRightBottom (l.x0, l.y0, l.x1, l.y1).expanded (0.2f), l.bold, l.italic));
}

// a polar part blended into an opaque image at design centre (cx, cy)
void blendPolar (Image::BitmapData& bd, float s, const sk::Polar& m, float cx, float cy, float rmax)
{
    const int x0 = jmax (0, (int) std::floor ((cx - rmax) * s)), x1 = jmin (bd.width - 1, (int) std::ceil ((cx + rmax) * s));
    const int y0 = jmax (0, (int) std::floor ((cy - rmax) * s)), y1 = jmin (bd.height - 1, (int) std::ceil ((cy + rmax) * s));
    float v[4];
    for (int py = y0; py <= y1; ++py)
        for (int px = x0; px <= x1; ++px)
        {
            if (! wavy::skin::polarAt (m, ((float) px + 0.5f) / s - cx, ((float) py + 0.5f) / s - cy, 0.0f, rmax, v)) continue;
            const float a = jlimit (0.0f, 1.0f, v[3]);
            if (a <= 1.0e-4f) continue;
            const auto c = bd.getPixelColour (px, py);
            const float rgb[3] = { v[0], v[1], v[2] };
            auto mix = [a] (uint8 d, float sv) { return (uint8) jlimit (0.0f, 255.0f, (float) d * (1.0f - a) + 255.0f * jlimit (0.0f, 1.0f, sv)); };
            bd.setPixelColour (px, py, Colour (mix (c.getRed(), rgb[0]), mix (c.getGreen(), rgb[1]), mix (c.getBlue(), rgb[2])));
        }
}

// a path's coverage at physical scale s (design units in, one alpha channel out)
Image coverage (const Path& p, int w, int h, float s)
{
    Image m (Image::SingleChannel, w, h, true, SoftwareImageType());
    Graphics g (m);
    g.addTransform (AffineTransform::scale (s));
    g.setColour (Colours::white);
    g.fillPath (p);
    return m;
}

float alphaAt (const Image::BitmapData& m, int x, int y)
{
    if (x < 0 || y < 0 || x >= m.width || y >= m.height) return 0.0f;
    return (float) *m.getPixelPointer (x, y) / 255.0f;
}

// a print engraved into the panel (the badge): the groove's floor darker than the panel, its upper wall in shadow and
// the lip below it lit (the reference's light from above; its grooves measured: floor 0.72 of the panel, upper wall
// 0.45, lower lip 1.22)
void engrave (Image::BitmapData& bd, const Path& p, float s)
{
    const auto m = coverage (p, bd.width, bd.height, s);
    const Image::BitmapData md (m, Image::BitmapData::readOnly);
    const auto r = (p.getBounds().expanded (3.0f) * s).getSmallestIntegerContainer().getIntersection ({ 0, 0, bd.width, bd.height });
    const int dx = jmax (1, roundToInt (0.6f * s)), dy = jmax (1, roundToInt (1.2f * s));
    for (int y = r.getY(); y < r.getBottom(); ++y)
        for (int x = r.getX(); x < r.getRight(); ++x)
        {
            const float a = alphaAt (md, x, y), above = alphaAt (md, x - dx, y - dy);
            const float k = (1.0f - a) * (1.0f + 0.22f * above) + a * (0.72f - 0.27f * (1.0f - above));
            if (std::abs (k - 1.0f) < 1.0e-3f) continue;
            const auto c = bd.getPixelColour (x, y);
            bd.setPixelColour (x, y, Colour ((uint8) jlimit (0.0f, 255.0f, c.getRed() * k), (uint8) jlimit (0.0f, 255.0f, c.getGreen() * k),
                                             (uint8) jlimit (0.0f, 255.0f, c.getBlue() * k)));
        }
}

// the suite's mark printed on the red plate the way the reference's maker's mark is: a pale ink that takes the plate's
// shading (measured: the ink = the plate + its red times (0.45, 1, 0.85), so it darkens with the plate toward the top)
void printOnPlate (Image::BitmapData& bd, Rectangle<float> r, float s)
{
    Image m (Image::ARGB, bd.width, bd.height, true, SoftwareImageType());
    {
        Graphics g (m);
        g.addTransform (AffineTransform::scale (s));
        wavy::drawMark (g, r, false, false, Colours::white, 1.5f);
    }
    const Image::BitmapData md (m, Image::BitmapData::readOnly);
    const auto box = (r.expanded (2.0f) * s).getSmallestIntegerContainer().getIntersection ({ 0, 0, bd.width, bd.height });
    for (int y = box.getY(); y < box.getBottom(); ++y)
        for (int x = box.getX(); x < box.getRight(); ++x)
        {
            const float a = md.getPixelColour (x, y).getFloatAlpha();
            if (a <= 1.0e-3f) continue;
            const auto c = bd.getPixelColour (x, y);
            const float red = (float) c.getRed();
            bd.setPixelColour (x, y, Colour ((uint8) jlimit (0.0f, 255.0f, c.getRed() + a * 0.45f * red), (uint8) jlimit (0.0f, 255.0f, c.getGreen() + a * 1.0f * red),
                                             (uint8) jlimit (0.0f, 255.0f, c.getBlue() + a * 0.85f * red)));
        }
}

// WVox Chain's name plate on the red header, in the reference's chrome lettering: a bold italic name (cap height 18 px
// from y 70, as the reference's signature) and a chrome bar under each word, filled with the chrome's measured shading
// (by height: a bright rim at the top, the bright band at the x-height, darkening to the foot's lit rim), outlined in
// the plate's darkest red and casting a shadow down and to the right
void drawNamePlate (Graphics& g)
{
    const float top = 70.0f, base = 88.0f, x0 = 63.0f;
    auto word = [&] (const String& w, float x)
    {
        GlyphArrangement ga;
        ga.addLineOfText (font (24.0f, true, true), w, 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        const auto b = p.getBounds();
        const float sy = (base - top) / b.getHeight(), sx = sy * 0.9f;   // (the reference's letters are narrower)
        p.applyTransform (AffineTransform::translation (-b.getX(), -b.getY()).scaled (sx, sy).translated (x, top));
        return p;
    };
    const auto first = word ("WVox", x0);
    const auto second = word ("Chain", first.getBounds().getRight() + 6.5f);
    Path name (first);
    name.addPath (second);
    // the bars: 3 px chrome under each word, their ends slanted as the letters
    Path bars;
    for (const auto* w : { &first, &second })
    {
        const auto b = w->getBounds();
        const float l = b.getX() - 3.0f, r = b.getRight() - 1.0f, y0 = 91.0f, y1 = 94.0f;
        Path bar;
        bar.startNewSubPath (l + 1.0f, y0); bar.lineTo (r + 1.0f, y0); bar.lineTo (r, y1); bar.lineTo (l, y1); bar.closeSubPath();
        bars.addPath (bar);
    }
    Path all (name);
    all.addPath (bars);
    // the shadow (soft: a few offsets), the dark outline, then the chrome
    for (auto o : { Point<float> (2.6f, 3.0f), Point<float> (1.8f, 2.2f), Point<float> (1.0f, 1.4f) })
    {
        g.setColour (Colour (22, 0, 4).withAlpha (0.35f));
        g.fillPath (all, AffineTransform::translation (o));
    }
    g.setColour (Colour (30, 2, 8));
    g.strokePath (all, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    ColourGradient chrome (Colour (172, 174, 178), 0.0f, top, Colour (127, 130, 133), 0.0f, base, false);
    struct Stop { float y; uint8 v; };
    for (auto st : { Stop { 71.5f, 197 }, Stop { 73.5f, 196 }, Stop { 75.0f, 141 }, Stop { 77.5f, 218 }, Stop { 80.0f, 188 },
                     Stop { 82.0f, 134 }, Stop { 84.0f, 90 }, Stop { 86.0f, 70 }, Stop { 87.2f, 98 } })
        chrome.addColour ((st.y - top) / (base - top), Colour (st.v, (uint8) jmin (255, st.v + 2), (uint8) jmin (255, st.v + 5)));
    g.setGradientFill (chrome);
    g.fillPath (name);
    ColourGradient bar (Colour (82, 84, 88), 0.0f, 91.0f, Colour (158, 162, 166), 0.0f, 93.6f, false);
    bar.addColour (0.55, Colour (140, 145, 149));
    g.setGradientFill (bar);
    g.fillPath (bars);
}

// the badge: "Vocals" engraved in an open oval (the stage's word, in the reference's place: letters x 617 .. 689, cap
// height 136 .. 155; the oval's groove from the lower left over the top to the right, a line under the word)
Path badgePath()
{
    // (the letters thickened by half a pixel all round: the reference's are heavier than a bold sans)
    const Path letters = fittedText ("Vocals", Rectangle<float>::leftTopRightBottom (617.5f, 136.5f, 689.0f, 155.0f), true, false);
    Path p (letters);
    PathStrokeType (1.0f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (p, letters);
    p.addPath (letters);
    Path ring;
    ring.startNewSubPath (608.5f, 155.5f);
    ring.cubicTo (603.0f, 146.0f, 604.5f, 131.0f, 622.0f, 127.0f);
    ring.cubicTo (637.0f, 122.0f, 655.0f, 122.0f, 668.0f, 127.5f);
    ring.cubicTo (673.0f, 129.5f, 676.5f, 131.5f, 678.5f, 133.5f);
    ring.startNewSubPath (612.5f, 159.5f);
    ring.lineTo (685.0f, 159.5f);
    ring.quadraticTo (689.5f, 159.0f, 690.0f, 153.5f);
    Path stroked;
    PathStrokeType (3.6f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (stroked, ring);
    p.addPath (stroked);
    return p;
}

// the static art at physical scale s: the panels and their grain, the window, the grille, the overlays (tracks, meter
// windows, seams), the corners and the screws; then the printing, the name plate, the mark and the badge
Image renderBackground (float s, bool stereo)
{
    const int bw = roundToInt (WVoxChainEditor::W * s), bh = roundToInt (WVoxChainEditor::H * s);
    Image img (Image::RGB, bw, bh, true);
    {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        struct Over { const wavy::skin::LowRank* m; float x, y; };
        std::vector<Over> overs;
        static constexpr int trackX[6] = { 186, 253, 319, 386, 452, 519 };
        for (size_t i = 0; i < 6; ++i) overs.push_back ({ sk::tracks[i], (float) (trackX[i] + sk::trackDx), (float) sk::trackY });
        overs.push_back ({ &sk::sensTrack, (float) (49 + sk::trackDx), (float) sk::sideTrackY });
        overs.push_back ({ &sk::outTrack, (float) (655 + sk::trackDx), (float) sk::sideTrackY });
        overs.push_back ({ &sk::meterWindowIn, (float) sk::meterInX, (float) sk::meterY });
        overs.push_back ({ &sk::meterWindowOut, (float) sk::meterOutX, (float) sk::meterY });
        for (const auto& sm : sk::seams) overs.push_back ({ sm.m, sm.x, sm.y });
        if (stereo) overs.push_back ({ &sk::stereoTab, (float) sk::stereoTabX, (float) sk::stereoTabY });
        float d[4];
        for (int py = 0; py < bh; ++py)
            for (int px = 0; px < bw; ++px)
            {
                const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                if (y < 19.0f) { bd.setPixelColour (px, py, Colours::black); continue; }
                float rgb[3];
                float n = 0.0f;
                if (x < 22.0f || x >= 716.0f)                               // the chrome rails (low-rank parts)
                {
                    const bool left = x < 22.0f;
                    wavy::skin::lowRankAt (left ? sk::railLeftPart : sk::railRightPart, left ? x : x - 716.0f, y - 19.0f, d);
                    for (int k = 0; k < 3; ++k) rgb[k] = 255.0f * d[k];
                }
                else if (x >= (float) sk::windowBox[0] && x < (float) sk::windowBox[2] && y >= (float) sk::windowBox[1] && y < (float) sk::windowBox[3])
                {
                    // the flavours' window (a low-rank part) and its dots
                    wavy::skin::lowRankAt (sk::windowPart, x - (float) sk::windowBox[0], y - (float) sk::windowBox[1], d);
                    float f = 1.0f;
                    if (x >= (float) sk::dotsBox[0] && x < (float) sk::dotsBox[2] && y >= (float) sk::dotsBox[1] && y < (float) sk::dotsBox[3])
                        f = sk::windowDots[(int) y & 1][(int) x & 1];
                    for (int k = 0; k < 3; ++k) rgb[k] = 255.0f * d[k] * f;
                }
                else if (x >= (float) sk::grilleBox[0] && x < (float) sk::grilleBox[2] && y >= (float) sk::grilleBox[1] && y < (float) sk::grilleBox[3])
                {
                    // the grille's lattice: its row's profile (period 11 px; linear between), the nearest hole's tint
                    const float fy = jlimit (0.0f, (float) (sk::grilleBox[3] - sk::grilleBox[1]) - 1.001f, y - 0.5f - (float) sk::grilleBox[1]);
                    const float fx = std::fmod (x - 0.5f - (float) sk::grilleBox[0] + 11.0f, 11.0f);
                    const int iy = (int) fy, ix = (int) fx, ix1 = (ix + 1) % 11;
                    const float ty = fy - (float) iy, tx = fx - (float) ix;
                    float t[3] = { 1.0f, 1.0f, 1.0f };
                    for (const auto& h : sk::grilleHoles)
                        if ((x - h[0]) * (x - h[0]) + (y - h[1]) * (y - h[1]) < 25.0f) { t[0] = h[2]; t[1] = h[3]; t[2] = h[4]; break; }
                    for (int k = 0; k < 3; ++k)
                    {
                        const auto& T = sk::grilleTile;
                        const float a = T[iy][ix][k] * (1.0f - tx) + T[iy][ix1][k] * tx, b = T[iy + 1][ix][k] * (1.0f - tx) + T[iy + 1][ix1][k] * tx;
                        rgb[k] = (a * (1.0f - ty) + b * ty) * t[k];
                    }
                }
                else
                {
                    const wavy::skin::Panel* p;
                    if (y < 107.0f) p = &sk::header;
                    else if (y < 181.0f) p = x < 164.0f ? &sk::grilleBars : x < 577.0f ? &sk::displayFrame : &sk::badge;
                    else p = x < 164.0f ? &sk::lowerLeft : x < 577.0f ? &sk::lowerMid : &sk::lowerRight;
                    wavy::skin::panelAt (*p, x, y, rgb);
                    // a fine grain, the rest of what the models leave (deterministic per design pixel)
                    uint32 hsh = (uint32) x * 374761393u + (uint32) y * 668265263u;
                    hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
                    hsh ^= hsh >> 16;
                    n = 0.8f * p->grain * ((float) (hsh & 0xffff) / 32767.5f - 1.0f);
                }
                for (auto& o : overs)
                    if (x >= o.x && y >= o.y && x < o.x + (float) o.m->w && y < o.y + (float) o.m->h)
                    {
                        wavy::skin::lowRankAt (*o.m, x - o.x, y - o.y, d);
                        for (int k = 0; k < 3; ++k) rgb[k] += 255.0f * d[k];
                    }
                // the corners (opaque parts, faded over 4 px into what is round them on their open sides)
                for (int c = 0; c < 4; ++c)
                {
                    const auto* cb = sk::corners[c];
                    if (x < (float) cb[0] || x >= (float) cb[2] || y < (float) cb[1] || y >= (float) cb[3]) continue;
                    const float fx = (c % 2 == 0) ? ((float) cb[2] - x) / 4.0f : (x - (float) cb[0]) / 4.0f;
                    const float fy = (c < 2) ? ((float) cb[3] - y) / 4.0f : (y - (float) cb[1]) / 4.0f;
                    const float w = jlimit (0.0f, 1.0f, fx) * jlimit (0.0f, 1.0f, fy);
                    static const wavy::skin::LowRank* parts[4] = { &sk::cornerTL, &sk::cornerTR, &sk::cornerBL, &sk::cornerBR };
                    wavy::skin::lowRankAt (*parts[c], x - (float) cb[0], y - (float) cb[1], d);
                    for (int k = 0; k < 3; ++k) rgb[k] = rgb[k] * (1.0f - w) + 255.0f * d[k] * w;
                    n *= 1.0f - w;
                }
                bd.setPixelColour (px, py, Colour ((uint8) jlimit (0.0f, 255.0f, rgb[0] + n), (uint8) jlimit (0.0f, 255.0f, rgb[1] + n),
                                                   (uint8) jlimit (0.0f, 255.0f, rgb[2] + n)));
            }
        for (const auto& sc : sk::screws) blendPolar (bd, s, sk::screw, sc[0], sc[1], 7.0f);
        blendPolar (bd, s, sk::headerScrewL, sk::headerScrews[0][0], sk::headerScrews[0][1], 8.5f);
        blendPolar (bd, s, sk::headerScrewR, sk::headerScrews[1][0], sk::headerScrews[1][1], 8.5f);
        printOnPlate (bd, Rectangle<float> (664.0f, 28.0f, 26.0f, 21.0f), s);
        engrave (bd, badgePath(), s);
    }
    Graphics g (img);
    g.addTransform (AffineTransform::scale (s));
    for (const auto& l : sk::labels) drawLabel (g, l);
    for (const auto& r : sk::runs)
    {
        g.setColour (Colour::greyLevel (r.grey / 255.0f));
        g.fillRect (r.x, r.y, r.w, r.h);
    }
    drawNamePlate (g);
    return img;
}

// a push button over the background (the background's pixels under it plus the button's difference to the
// background the skin was cut from), at physical scale s; its top left at design (x, y)
Image renderButton (const Image& background, float s, float x, float y, bool lit)
{
    const auto& m = lit ? sk::buttonOnDelta : sk::buttonOffDelta;
    const auto r = Rectangle<int> (roundToInt (x * s), roundToInt (y * s), roundToInt ((float) m.w * s), roundToInt ((float) m.h * s))
                       .getIntersection (background.getBounds());
    Image img = background.getClippedImage (r).createCopy();
    Image::BitmapData bd (img, Image::BitmapData::readWrite);
    float d[4];
    for (int py = 0; py < bd.height; ++py)
        for (int px = 0; px < bd.width; ++px)
        {
            const float lx = ((float) (r.getX() + px) + 0.5f) / s - x, ly = ((float) (r.getY() + py) + 0.5f) / s - y;
            const float fx = jmin (lx - (float) sk::buttonSupport[0], (float) sk::buttonSupport[2] - lx);
            const float fy = jmin (ly - (float) sk::buttonSupport[1], (float) sk::buttonSupport[3] - ly);
            const float w = jlimit (0.0f, 1.0f, jmin (fx, fy) + 0.5f);
            if (w <= 0.0f) continue;
            wavy::skin::lowRankAt (m, lx, ly, d);
            const auto c = bd.getPixelColour (px, py);
            auto add = [&] (uint8 v, float dv) { return (uint8) jlimit (0.0f, 255.0f, (float) v + 255.0f * dv * w); };
            bd.setPixelColour (px, py, Colour (add (c.getRed(), d[0]), add (c.getGreen(), d[1]), add (c.getBlue(), d[2])));
        }
    return img;
}

// the faders' travel: the control's 209 px less the cap's 58 (7.55 px per step of the -10 .. +10 scale, measured)
constexpr float kTravel = 209.0f - 58.0f;
constexpr int selectorX[6] = { 176, 243, 309, 376, 442, 508 };
}

//==============================================================================
Fader::Fader (bool sensitivityCap, Marks& m) : sens (sensitivityCap), marks (m)
{
    setSliderStyle (LinearVertical);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);                          // (a click on the cap leaves the value: measured)
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

float Fader::capTop() const
{
    const double f = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    return (float) ((1.0 - f) * kTravel);
}

void Fader::paint (Graphics& g)
{
    const float s = physicalScale (g), y = capTop();
    drawSprite (g, sprite (shadow, s, [] (float sc) { return wavy::skin::renderLowRank (sk::capShadow, sc); }), s, 0.0f, y);
    drawSprite (g, sprite (cap, s, [this] (float sc) { return wavy::skin::renderLowRank (sens ? sk::capSens : sk::cap, sc); }), s, 0.0f, y);
    if (marks.selected == this)                                     // the marked cap's frame (the reference's Active image)
    {
        g.setColour (activeColour);
        g.drawRect (Rectangle<float> (0.0f, y, 34.0f, 58.0f), 1.0f);
    }
}

void Fader::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (! onCap (e.position)) { marks.clear(); return; }            // the track off the cap: nothing (but unmarks)
    if (e.mods.isShiftDown() && marks.selected == this) { marks.clear(); return; }
    marks.select (this);
    if (e.mods.isAltDown()) { setValue (getDoubleClickReturnValue(), sendNotificationSync); repaint(); return; }
    dragging = true;
    start = getValue();
    origin = last = virt = e.position;
    e.source.enableUnboundedMouseMovement (true, false);
    Slider::mouseDown (e);
    repaint();
}

void Fader::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    const auto d = e.position - last;
    last = e.position;
    const float k = wavy::ctrlHeldNow() ? (std::abs (d.y) <= 2.0f ? 0.037f : 0.19f) : 1.0f;
    virt += d * k;
    const double perUnit = kTravel / (getMaximum() - getMinimum());
    setValue (jlimit (getMinimum(), getMaximum(), start + (double) (origin.y - virt.y) / perUnit), sendNotificationSync);
}

void Fader::mouseUp (const MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    Slider::mouseUp (e);
    e.source.enableUnboundedMouseMovement (false);
    const double perUnit = kTravel / (getMaximum() - getMinimum());
    const auto at = Point<float> (origin.x, origin.y - (float) ((getValue() - start) * perUnit));
    Desktop::setMousePosition (localPointToGlobal (at).roundToInt());
    if (marks.changed) marks.changed();
    repaint();
}

void Fader::mouseDoubleClick (const MouseEvent& e)
{
    if (onCap (e.position) && marks.enter) marks.enter (this);
}

//==============================================================================
ColourSelector::ColourSelector (RangedAudioParameter& param, const Stage& st, int i, Marks& m) : p (param), stage (st), index (i), marks (m)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

int ColourSelector::state() const { return roundToInt (p.convertFrom0to1 (p.getValue())); }

void ColourSelector::setState (int st)
{
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 ((float) jlimit (0, 3, st)));
    p.endChangeGesture();
    repaint();
}

void ColourSelector::paint (Graphics& g)
{
    const int st = state();
    // the flavours, the third at the top, each in its ink box measured on the reference; the selected one in its
    // colour (measured: green (100, 192, 72), blue (68, 160, 202), red (255, 60, 60); the others (74, 74, 74))
    static const Colour lit[4] = { Colour (74, 74, 74), Colour (100, 192, 72), Colour (68, 160, 202), Colour (255, 60, 60) };
    for (int row = 0; row < 3; ++row)
    {
        const int k = 3 - row;
        const auto* b = sk::nameBoxes[index][row];
        g.setColour (st == k ? lit[k] : lit[0]);
        g.fillPath (fittedText (String (stage.names[k]).toUpperCase(), Rectangle<float>::leftTopRightBottom (b[0], b[1], b[2], b[3]).expanded (0.15f), true, true));
    }
    if (marks.selected == this)                                     // the marked button's ring (the reference's Active image)
    {
        g.setColour (activeColour);
        g.drawRect (Rectangle<float> (11.0f, 50.0f, 32.0f, 20.0f), 1.0f);
    }
}

void ColourSelector::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.y < 40)
    {
        // the names (the reference's fixed-state rows, 12 px each, the third at the top): a click selects one, a click
        // on the selected one turns the colour off; the marked control is unmarked
        marks.clear();
        const int k = jlimit (1, 3, 3 - (e.y - 1) / 12);
        setState (state() == k ? 0 : k);
        return;
    }
    if (e.mods.isShiftDown() && marks.selected == this) { marks.clear(); return; }
    marks.select (this);
    if (e.mods.isAltDown()) { setState (1); return; }                  // (the default colour)
    setState ((state() + 1) % 4);
}

//==============================================================================
void Meter::set (float l, float r, double dt)
{
    const float v[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        shown[c] = ballistics[c].update (v[c], dt);
        if (v[c] >= 0.0f) clip[c] = true;
    }
    repaint();
}

void Meter::paint (Graphics& g)
{
    const float s = physicalScale (g);
    // the bar: 155 px for 0 .. -24 dB below a top margin (the scale's 0 at y 10 and 24 at y 149 of the bar; linear in dB)
    auto lit = [] (float db) { return jlimit (0.0f, 155.0f, 149.0f - (jlimit (-26.0f, 0.0f, db) + 24.0f) / 24.0f * 139.0f); };
    auto bar = [&] (Sprite& offS, Sprite& onS, const wavy::skin::LowRank& offM, const wavy::skin::LowRank& onM, float x, float db)
    {
        drawSprite (g, sprite (offS, s, [&offM] (float sc) { return wavy::skin::renderLowRank (offM, sc); }), s, x, 10.0f);
        const float top = lit (db);
        if (top >= 155.0f) return;
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (Rectangle<float> (x, 10.0f + top, (float) onM.w, 155.0f - top).getSmallestIntegerContainer());
        drawSprite (g, sprite (onS, s, [&onM] (float sc) { return wavy::skin::renderLowRank (onM, sc); }), s, x, 10.0f);
    };
    auto clipLight = [&] (Sprite& offS, Sprite& onS, const wavy::skin::LowRank& offM, const wavy::skin::LowRank& onM, float x, bool c)
    {
        drawSprite (g, c ? sprite (onS, s, [&onM] (float sc) { return wavy::skin::renderLowRank (onM, sc); })
                         : sprite (offS, s, [&offM] (float sc) { return wavy::skin::renderLowRank (offM, sc); }), s, x, 0.0f);
    };
    if (mono)
    {
        bar (monoOff, monoOn, sk::meterMonoOff, sk::meterMonoOn, 0.0f, shown[0]);
        clipLight (clipMonoOff, clipMonoOn, sk::clipMonoOff, sk::clipMonoOn, 0.0f, clip[0]);
    }
    else
        for (int c = 0; c < 2; ++c)                                     // (one sprite set serves both channels)
        {
            bar (off, on, sk::meterOff, sk::meterOn, c == 0 ? 0.0f : 22.0f, shown[c]);
            clipLight (clipOff, clipOn, sk::clipOff, sk::clipOn, c == 0 ? 0.0f : 22.0f, clip[c]);
        }
}

//==============================================================================
void SensLed::set (float levelDb)
{
    // the reference's LED curve (ParamXML X / Y: the level -144 / -9 / -3 / 3 / 6 dB to -144 / -108 / -72 / -36 / 0)
    // held as a peak that falls at the meters' rate; the frame from the curve's output
    held = jmax (levelDb, held - 0.5f);
    static const float X[5] = { -144.0f, -9.0f, -3.0f, 3.0f, 6.0f }, Y[5] = { -144.0f, -108.0f, -72.0f, -36.0f, 0.0f };
    float y = Y[0];
    for (int i = 0; i < 4; ++i)
        if (held >= X[i]) y = Y[i] + (Y[i + 1] - Y[i]) * jlimit (0.0f, 1.0f, (held - X[i]) / (X[i + 1] - X[i]));
    const int f = jlimit (0, 23, roundToInt ((y + 144.0f) / 144.0f * 23.0f));
    if (f != frame) { frame = f; repaint(); }
}

void SensLed::paint (Graphics& g)
{
    const float s = physicalScale (g);
    drawSprite (g, sprite (frames[(size_t) frame], s, [this] (float sc) { return wavy::skin::renderLowRank (*sk::sensLeds[frame], sc); }), s, 0.0f, 0.0f);
}

//==============================================================================
BalanceKnob::BalanceKnob (Marks& m) : marks (m)
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setRotaryParameters (degreesToRadians (360.0f + sk::balanceFrom), degreesToRadians (360.0f + sk::balanceTo), true);
    setMouseDragSensitivity (500);                                    // (halved on small mouse steps: 1/1000 of the range per px)
    setVelocityModeParameters (1.0, 1, 0.0, false);
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void BalanceKnob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.mods.isShiftDown() && marks.selected == this) { marks.clear(); return; }
    marks.select (this);
    feel.down (*this, e, {}, nullptr);
    repaint();
}

void BalanceKnob::paint (Graphics& g)
{
    const float s = physicalScale (g);
    const int f = jlimit (0, 71, roundToInt (valueToProportionOfLength (getValue()) * 71.0));
    const float angle = sk::balanceFrom + (sk::balanceTo - sk::balanceFrom) * (float) f / 71.0f;
    drawSprite (g, sprite (frames[(size_t) f], s, [angle] (float sc)
                { return wavy::skin::renderPolar (sk::balanceKnob, 30, 30, sk::balanceKnob.cx, sk::balanceKnob.cy, sc, angle, 15.0f); }), s, 0.0f, 0.0f);
    if (marks.selected == this)                                     // the marked knob's ring (the reference's Active image: r 7.54 round (15, 14))
    {
        g.setColour (activeColour);
        g.drawEllipse (Rectangle<float> (15.0f - 7.54f, 14.0f - 7.54f, 15.08f, 15.08f), 1.0f);
    }
}
}

//==============================================================================
using namespace wvoxchain::ui;

WVoxChainEditor::WVoxChainEditor (WVoxChainProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wvoxchain::mode), "WVox Chain", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Classic, wvoxchain::modeNames()),
      proc (p),
      inMeter (make<Meter> (marks)), outMeter (make<Meter> (marks)), led (make<SensLed> (marks)), balance (make<BalanceKnob> (marks))
{
    for (auto* c : getChildren())
        if (dynamic_cast<ResizableCornerComponent*> (c) != nullptr) c->setAlpha (0.0f);
    setFactoryPresets (wvoxchain::factoryPresets());
    setKeepOnPresetLoad ({ p.apvts.getParameter (wvoxchain::sensitivity), p.apvts.getParameter (wvoxchain::output) });   // (IgnoreOnPresetLoad)
    marks.changed = [this] { content().repaint(); };
    marks.enter = [this] (Slider* sl) { showEntry (sl); };

    // the faders: the control rectangles (34 x 209 from y 210), the cap's shadow reaching 9 px right of them
    const std::array<std::pair<const char*, int>, 8> fd { { { wvoxchain::sensitivity, 49 }, { wvoxchain::bass, 186 }, { wvoxchain::treble, 253 },
        { wvoxchain::compress, 319 }, { wvoxchain::reverb, 386 }, { wvoxchain::delay, 452 }, { wvoxchain::pitch, 519 }, { wvoxchain::output, 655 } } };
    for (size_t i = 0; i < fd.size(); ++i)
    {
        auto& f = make<Fader> (i == 0, marks);
        attachSlider (f, fd[i].first, { fd[i].second, 210, 43, 215 }, 0.0);
        faders.push_back (&f);
    }
    // the colour selectors (55 x 81 from y 117, over the six channel faders)
    for (size_t i = 0; i < 6; ++i)
    {
        const auto& st = wvoxchain::stages()[i];
        auto& c = make<wvoxchain::ui::ColourSelector> (*p.apvts.getParameter (st.colour), st, (int) i, marks);
        addToContent (c, { selectorX[i], 117, 55, 81 });
        auto* att = new ParameterAttachment (*p.apvts.getParameter (st.colour), [this, &c] (float) { c.repaint(); content().repaint(); }, &p.undo);
        keepAttachment (std::unique_ptr<ParameterAttachment> (att));
        colours.push_back (&c);
    }
    addToContent (inMeter, { 109, 227, 31, 175 });
    addToContent (outMeter, { 599, 227, 31, 175 });
    addToContent (led, { 52, 189, 28, 25 });
    attachSlider (balance, wvoxchain::balance, { 109, 395, 30, 30 }, 0.0);
    for (auto* f : faders) f->setDoubleClickReturnValue (true, 0.0);
    balance.setDoubleClickReturnValue (true, 0.0);
    layoutForMode();
}

WVoxChainEditor::~WVoxChainEditor()
{
    marks.changed = nullptr;
    entry.reset();
}

void WVoxChainEditor::layoutForMode()
{
    const bool mono = wvoxchain::isMonoIn (proc.modeIndex());
    if ((int) mono == laidOut) return;
    laidOut = (int) mono;
    balance.setVisible (! mono);
    if (mono && marks.selected == &balance) marks.clear();
    inMeter.mono = mono;
    content().repaint();
}

Rectangle<int> WVoxChainEditor::valueBox (const Slider* sl) const
{
    if (sl == &balance) return { 106, 454, 37, 13 };
    for (size_t i = 0; i < faders.size(); ++i)
        if (faders[i] == sl) return { faders[i]->getX() - 1, 454, 37, 13 };
    return {};
}

void WVoxChainEditor::paintContent (Graphics& g)
{
    const bool stereo = ! wvoxchain::isMonoIn (proc.modeIndex());
    const float s = physicalScale (g);
    if (background.isNull() || std::abs (s - backgroundScale) > 1.0e-3f || backgroundMode != (int) stereo)
    {
        background = renderBackground (s, stereo);
        backgroundScale = s;
        backgroundMode = (int) stereo;
        buttonLit.fill (-1);
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.setOpacity (1.0f);
        g.drawImageAt (background, 0, 0);
        // the push buttons, on the background's own pixel grid (unlit while the stage's colour is off)
        for (size_t i = 0; i < 6; ++i)
        {
            const int on = colours[i]->state() != 0 ? 1 : 0;
            if (buttonLit[i] != on) { buttons[i] = renderButton (background, s, (float) selectorX[i], 117.0f + (float) sk::buttonY, on != 0); buttonLit[i] = on; }
            g.drawImageAt (buttons[i], roundToInt ((float) selectorX[i] * s), roundToInt ((117.0f + (float) sk::buttonY) * s));
        }
    }
    // the value under the marked control while the mouse is on it (or while it is dragged): the reference's box (black
    // at 0.8 in a white frame; the digits 7 px tall, grey 200)
    auto box = [&] (Slider* sl, const String& text)
    {
        const auto r = valueBox (sl).toFloat();
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRect (r);
        g.setColour (Colours::white);
        g.drawRect (r, 1.0f);
        g.setColour (Colour::greyLevel (200.0f / 255.0f));
        g.setFont (font (9.8f, false, false));
        g.drawText (text, r, Justification::centred);
    };
    for (auto* f : faders)
        if ((marks.showsValue (f) || f->dragging) && f != entryFor) box (f, String (f->getValue(), 1));
    if (balance.isVisible() && (marks.showsValue (&balance) || balance.dragging()) && entryFor != &balance) box (&balance, String (balance.getValue(), 1));
}

void WVoxChainEditor::tick()
{
    layoutForMode();
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = lastTick > 0.0 ? now - lastTick : 1.0 / 60.0;
    lastTick = now;
    inMeter.set (proc.inL.take(), proc.inR.take(), dt);
    outMeter.set (proc.outL.take(), proc.outR.take(), dt);
    led.set (proc.sensLevelDb.load());
}

void WVoxChainEditor::showEntry (Slider* sl)
{
    entry = std::make_unique<TextEditor>();
    entryFor = sl;
    auto& e = *entry;
    e.setJustification (Justification::centred);
    e.setFont (font (9.8f, false, false));
    e.setColour (TextEditor::backgroundColourId, Colours::black);
    e.setColour (TextEditor::textColourId, Colours::white);
    e.setColour (TextEditor::highlightColourId, Colour (0, 90, 200));
    e.setColour (TextEditor::highlightedTextColourId, Colours::white);
    e.setColour (CaretComponent::caretColourId, Colours::white);
    e.setColour (TextEditor::outlineColourId, Colours::white);
    e.setColour (TextEditor::focusedOutlineColourId, Colours::white);
    e.setText (String (sl->getValue(), 1), false);
    e.selectAll();
    addToContent (e, valueBox (sl));
    auto close = [this, sl] (bool apply)
    {
        if (apply && entry != nullptr)
        {
            const auto t = entry->getText().trim();
            if (t.containsAnyOf ("0123456789"))
            {
                const double v = t.getDoubleValue();
                if (v >= sl->getMinimum() && v <= sl->getMaximum()) sl->setValue (v, sendNotificationSync);   // (out of range: ignored)
            }
        }
        MessageManager::callAsync ([this] { entry.reset(); entryFor = nullptr; content().repaint(); });
    };
    e.onReturnKey = [close] { close (true); };
    e.onEscapeKey = [close] { close (false); };
    e.onFocusLost = [close] { close (true); };
    e.grabKeyboardFocus();
}
