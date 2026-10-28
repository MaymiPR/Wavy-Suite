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

// a printed label: the font's outline fitted into the ink box measured on the reference
void drawLabel (Graphics& g, const sk::Label& l)
{
    GlyphArrangement ga;
    ga.addLineOfText (font (12.0f, l.bold, l.italic), String::fromUTF8 (l.text), 0.0f, 0.0f);
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

// a polar part blended into an opaque image at design centre (cx, cy), its colours times tint
void blendPolar (Image::BitmapData& bd, float s, const sk::Polar& m, float cx, float cy, float rmax, const float tint[3] = nullptr)
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
            float rgb[3] = { v[0], v[1], v[2] };
            if (tint != nullptr) for (int k = 0; k < 3; ++k) rgb[k] *= tint[k];
            auto mix = [a] (uint8 d, float sv) { return (uint8) jlimit (0.0f, 255.0f, (float) d * (1.0f - a) + 255.0f * sv); };
            bd.setPixelColour (px, py, Colour (mix (c.getRed(), rgb[0]), mix (c.getGreen(), rgb[1]), mix (c.getBlue(), rgb[2])));
        }
}

// the static art at physical scale s: the panels and their grain, the overlays (tracks, meter windows), the screws and
// the grille's holes; then the printing and the name plate
Image renderBackground (float s, bool stereo)
{
    const int bw = roundToInt (WVoxChainEditor::W * s), bh = roundToInt (WVoxChainEditor::H * s);
    Image img (Image::RGB, bw, bh, true);
    {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        struct Over { const wavy::skin::LowRank* m; float x, y; };
        std::vector<Over> overs;
        for (size_t i = 1; i < 7; ++i)
            overs.push_back ({ &sk::track, (float) (std::array<int, 6> { 186, 253, 319, 386, 452, 519 }[i - 1] + sk::trackDx), (float) sk::trackY });
        overs.push_back ({ &sk::sensTrack, (float) (49 + sk::trackDx), (float) sk::sideTrackY });
        overs.push_back ({ &sk::outTrack, (float) (655 + sk::trackDx), (float) sk::sideTrackY });
        for (int x : { sk::meterInX, sk::meterOutX }) overs.push_back ({ &sk::meterWindow, (float) x, (float) sk::meterY });
        float d[4];
        for (int py = 0; py < bh; ++py)
            for (int px = 0; px < bw; ++px)
            {
                const float x = ((float) px + 0.5f) / s, y = ((float) py + 0.5f) / s;
                if (y < 19.0f) { bd.setPixelColour (px, py, Colours::black); continue; }
                if (x < 22.0f || x >= 716.0f)                                   // the chrome rails (low-rank parts)
                {
                    const bool left = x < 22.0f;
                    wavy::skin::lowRankAt (left ? sk::railLeftPart : sk::railRightPart, left ? x : x - 716.0f, y - 19.0f, d);
                    bd.setPixelColour (px, py, Colour::fromFloatRGBA (jlimit (0.0f, 1.0f, d[0]), jlimit (0.0f, 1.0f, d[1]), jlimit (0.0f, 1.0f, d[2]), 1.0f));
                    continue;
                }
                const wavy::skin::Panel* p;
                if (y < 107.0f) p = &sk::header;
                else if (y < 181.0f)
                {
                    if (x < 164.0f) p = &sk::grilleBars;
                    else if (x < 577.0f) p = (x >= 172.0f && x < 568.0f && y >= 112.0f && y < 157.0f) ? &sk::displayWindow : &sk::displayFrame;
                    else p = &sk::badge;
                }
                else p = x < 164.0f ? &sk::lowerLeft : x < 577.0f ? &sk::lowerMid : &sk::lowerRight;
                float rgb[3];
                wavy::skin::panelAt (*p, x, y, rgb);
                // a fine grain, the rest of what the models leave (deterministic per design pixel)
                uint32 hsh = (uint32) x * 374761393u + (uint32) y * 668265263u;
                hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
                hsh ^= hsh >> 16;
                const float n = 0.8f * p->grain * ((float) (hsh & 0xffff) / 32767.5f - 1.0f);
                for (auto& o : overs)
                    if (x >= o.x && y >= o.y && x < o.x + (float) o.m->w && y < o.y + (float) o.m->h)
                    {
                        wavy::skin::lowRankAt (*o.m, x - o.x, y - o.y, d);
                        for (int k = 0; k < 3; ++k) rgb[k] += 255.0f * d[k];
                    }
                bd.setPixelColour (px, py, Colour ((uint8) jlimit (0.0f, 255.0f, rgb[0] + n), (uint8) jlimit (0.0f, 255.0f, rgb[1] + n),
                                                   (uint8) jlimit (0.0f, 255.0f, rgb[2] + n)));
            }
        for (const auto& sc : sk::screws) blendPolar (bd, s, sk::screw, sc[0], sc[1], 7.0f);
        for (const auto& sc : sk::headerScrews) blendPolar (bd, s, sk::headerScrew, sc[0], sc[1], 8.5f);
        for (const auto& h : sk::grilleHoles) blendPolar (bd, s, sk::grilleHole, h[0], h[1], 5.5f, h + 2);
    }
    Graphics g (img);
    g.addTransform (AffineTransform::scale (s));
    for (const auto& l : sk::labels) drawLabel (g, l);
    for (const auto& r : sk::runs)
    {
        g.setColour (Colour::greyLevel (r.grey / 255.0f));
        g.fillRect (r.x, r.y, r.w, r.h);
    }
    if (stereo)                                                         // Balance's L and R (Stereo only)
    {
        g.setColour (Colour::greyLevel (0.55f));
        g.setFont (font (10.0f, false, true));
        g.drawText ("L", Rectangle<float> (99.0f, 401.0f, 10.0f, 12.0f), Justification::centred);
        g.drawText ("R", Rectangle<float> (139.0f, 401.0f, 10.0f, 12.0f), Justification::centred);
    }
    // WVox Chain's name plate on the red header: the name in a chrome-like italic over its shadow, and the suite's mark
    const auto plate = Rectangle<float> (60.0f, 62.0f, 260.0f, 36.0f);
    g.setFont (font (25.0f, true, true));
    g.setColour (Colour (40, 4, 8).withAlpha (0.8f));
    g.drawText ("WVox Chain", plate.translated (1.5f, 2.0f), Justification::centredLeft);
    {
        Path p;
        GlyphArrangement ga;
        ga.addLineOfText (font (25.0f, true, true), "WVox Chain", plate.getX(), plate.getCentreY() + 9.0f);
        ga.createPath (p);
        const auto b = p.getBounds();
        g.setGradientFill (ColourGradient (Colour (240, 240, 244), 0.0f, b.getY(), Colour (150, 152, 160), 0.0f, b.getBottom(), false));
        g.fillPath (p);
        g.setColour (Colour (60, 60, 66).withAlpha (0.7f));
        g.strokePath (p, PathStrokeType (0.6f));
    }
    wavy::drawMark (g, Rectangle<float> (662.0f, 28.0f, 30.0f, 22.0f), false, false, Colour (215, 200, 200));
    // the badge: the stage's word engraved in an oval in the right-hand panel
    {
        const auto ov = Rectangle<float> (604.0f, 129.0f, 92.0f, 32.0f);
        g.setColour (Colour (18, 18, 20).withAlpha (0.7f));
        g.drawRoundedRectangle (ov, 15.0f, 2.0f);
        g.setColour (Colour (110, 110, 114).withAlpha (0.35f));
        g.drawRoundedRectangle (ov.translated (0.0f, 1.2f), 15.0f, 1.0f);
    }
    g.setFont (font (24.0f, true, true));
    g.setColour (Colour (20, 20, 22).withAlpha (0.75f));
    g.drawText ("Vocals", Rectangle<float> (590.0f, 126.0f, 120.0f, 38.0f), Justification::centred);
    g.setColour (Colour (120, 120, 124).withAlpha (0.35f));
    g.drawText ("Vocals", Rectangle<float> (590.0f, 127.0f, 120.0f, 38.0f), Justification::centred);
    return img;
}

// the faders' travel: the control's 209 px less the cap's 58 (7.55 px per step of the -10 .. +10 scale, measured)
constexpr float kTravel = 209.0f - 58.0f;
}

//==============================================================================
Fader::Fader (bool sensitivityCap) : sens (sensitivityCap)
{
    setSliderStyle (LinearVertical);
    setTextBoxStyle (NoTextBox, true, 0, 0);
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
    if (dragging)                                                         // the active cap's yellow frame (the reference's)
    {
        g.setColour (Colour (255, 230, 0));
        g.drawRect (Rectangle<float> (0.0f, y, 34.0f, 58.0f), 1.0f);
    }
}

void Fader::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.mods.isAltDown()) { setValue (getDoubleClickReturnValue(), sendNotificationSync); return; }
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
    repaint();
}

//==============================================================================
ColourSelector::ColourSelector (RangedAudioParameter& param, const Stage& st) : p (param), stage (st)
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
    const float s = physicalScale (g);
    const int st = state();
    drawSprite (g, sprite (window, s, [] (float sc) { return wavy::skin::renderLowRank (sk::colourWindow, sc); }), s, 0.0f, 0.0f);
    drawSprite (g, st == 0 ? sprite (buttonOff, s, [] (float sc) { return wavy::skin::renderLowRank (sk::buttonOff, sc); })
                           : sprite (buttonOn, s, [] (float sc) { return wavy::skin::renderLowRank (sk::buttonOn, sc); }), s, 0.0f, 42.0f);
    // the button's selection frame (the reference's idle frame: black)
    g.setColour (Colours::black);
    g.drawRect (Rectangle<float> (12.0f, 50.0f, 31.0f, 19.0f), 1.0f);
    // the flavours, the third at the top; the selected one in its colour (measured: green (90, 172, 65), blue
    // (65, 157, 200), red (252, 56, 56); the others (71, 72, 72))
    static const Colour lit[4] = { Colour (71, 72, 72), Colour (110, 200, 80), Colour (80, 180, 225), Colour (252, 56, 56) };
    for (int k = 1; k <= 3; ++k)
    {
        const float base = 8.0f + 12.0f * (float) (3 - k);
        g.setColour (st == k ? lit[k] : lit[0]);
        // bold italic capitals 7 px tall, narrowed to the window if they are wider (the reference's are condensed)
        GlyphArrangement ga;
        ga.addLineOfText (font (10.5f, true, true), String (stage.names[k]).toUpperCase(), 0.0f, 0.0f);
        Path ink;
        ga.createPath (ink);
        const auto b = ink.getBounds();
        if (b.isEmpty()) continue;
        const float sy = 7.0f / b.getHeight(), sx = jmin (sy, 45.0f / b.getWidth());
        ink.applyTransform (AffineTransform::translation (-b.getCentreX(), -b.getBottom()).scaled (sx, sy).translated (27.5f, base));
        g.fillPath (ink);
    }
}

void ColourSelector::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.mods.isAltDown()) { setState (1); return; }                       // (the default colour)
    if (e.y < 40)
    {
        // the names (the reference's fixed-state rows, 12 px each, the third at the top): a click selects one, a click
        // on the selected one turns the colour off
        const int k = jlimit (1, 3, 3 - (e.y - 1) / 12);
        setState (state() == k ? 0 : k);
    }
    else setState ((state() + 1) % 4);
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
        for (int c = 0; c < 2; ++c)
        {
            Sprite& o = c == 0 ? off : on;                                  // (one sprite set serves both channels)
            ignoreUnused (o);
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
BalanceKnob::BalanceKnob()
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setRotaryParameters (degreesToRadians (360.0f + sk::balanceFrom), degreesToRadians (360.0f + sk::balanceTo), true);
    setMouseDragSensitivity (500);
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void BalanceKnob::paint (Graphics& g)
{
    const float s = physicalScale (g);
    const int f = jlimit (0, 71, roundToInt (valueToProportionOfLength (getValue()) * 71.0));
    const float angle = sk::balanceFrom + (sk::balanceTo - sk::balanceFrom) * (float) f / 71.0f;
    drawSprite (g, sprite (frames[(size_t) f], s, [angle] (float sc)
                { return wavy::skin::renderPolar (sk::balanceKnob, 30, 30, sk::balanceKnob.cx, sk::balanceKnob.cy, sc, angle, 15.0f); }), s, 0.0f, 0.0f);
    if (isMouseButtonDown())
    {
        g.setColour (Colour (255, 230, 0));
        g.drawEllipse (Rectangle<float> (2.0f, 2.0f, 26.0f, 26.0f), 1.0f);
    }
}
}

//==============================================================================
using namespace wvoxchain::ui;

WVoxChainEditor::WVoxChainEditor (WVoxChainProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wvoxchain::mode), "WVox Chain", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Classic, wvoxchain::modeNames()),
      proc (p),
      inMeter (make<Meter>()), outMeter (make<Meter>()), led (make<SensLed>()), balance (make<BalanceKnob>())
{
    for (auto* c : getChildren())
        if (dynamic_cast<ResizableCornerComponent*> (c) != nullptr) c->setAlpha (0.0f);
    setFactoryPresets (wvoxchain::factoryPresets());
    setKeepOnPresetLoad ({ p.apvts.getParameter (wvoxchain::sensitivity), p.apvts.getParameter (wvoxchain::output) });   // (IgnoreOnPresetLoad)

    // the faders: the control rectangles (34 x 209 from y 210), the cap's shadow reaching 9 px right of them
    const std::array<std::pair<const char*, int>, 8> fd { { { wvoxchain::sensitivity, 49 }, { wvoxchain::bass, 186 }, { wvoxchain::treble, 253 },
        { wvoxchain::compress, 319 }, { wvoxchain::reverb, 386 }, { wvoxchain::delay, 452 }, { wvoxchain::pitch, 519 }, { wvoxchain::output, 655 } } };
    for (size_t i = 0; i < fd.size(); ++i)
    {
        auto& f = make<Fader> (i == 0);
        f.enter = [this] (Slider* sl) { showEntry (sl); };
        f.hoverChanged = [this] { content().repaint(); };
        attachSlider (f, fd[i].first, { fd[i].second, 210, 43, 215 }, 0.0);
        faders.push_back (&f);
    }
    // the colour selectors (55 x 81 from y 117, over the six channel faders)
    for (size_t i = 0; i < 6; ++i)
    {
        const auto& st = wvoxchain::stages()[i];
        auto& c = make<wvoxchain::ui::ColourSelector> (*p.apvts.getParameter (st.colour), st);
        addToContent (c, { std::array<int, 6> { 176, 243, 309, 376, 442, 508 }[i], 117, 55, 81 });
        auto* att = new ParameterAttachment (*p.apvts.getParameter (st.colour), [&c] (float) { c.repaint(); }, &p.undo);
        keepAttachment (std::unique_ptr<ParameterAttachment> (att));
        colours.push_back (&c);
    }
    addToContent (inMeter, { 109, 227, 31, 175 });
    addToContent (outMeter, { 599, 227, 31, 175 });
    addToContent (led, { 52, 189, 28, 25 });
    balance.enter = [this] (Slider* sl) { showEntry (sl); };
    balance.hoverChanged = [this] { content().repaint(); };
    attachSlider (balance, wvoxchain::balance, { 109, 395, 30, 30 }, 0.0);
    for (auto* f : faders) f->setDoubleClickReturnValue (true, 0.0);
    balance.setDoubleClickReturnValue (true, 0.0);
    layoutForMode();
}

WVoxChainEditor::~WVoxChainEditor()
{
    entry.reset();
}

void WVoxChainEditor::layoutForMode()
{
    const bool mono = wvoxchain::isMonoIn (proc.modeIndex());
    if ((int) mono == laidOut) return;
    laidOut = (int) mono;
    balance.setVisible (! mono);
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
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.0f / s));
        g.drawImageAt (background, 0, 0);
    }
    // the value under the control the mouse is on (or that is being dragged)
    auto box = [&] (Slider* sl, const String& text)
    {
        const auto r = valueBox (sl).toFloat();
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRect (r);
        g.setColour (Colours::white);
        g.drawRect (r, 1.0f);
        g.setColour (Colour::greyLevel (200.0f / 255.0f));
        g.setFont (font (11.0f, true, false));
        g.drawText (text, r, Justification::centred);
    };
    for (auto* f : faders)
        if ((f->hover || f->dragging) && f != entryFor) box (f, String (f->getValue(), 1));
    if (balance.isVisible() && (balance.hover || balance.isMouseButtonDown()) && entryFor != &balance) box (&balance, String (balance.getValue(), 1));
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
    e.setFont (font (11.0f, true, false));
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
