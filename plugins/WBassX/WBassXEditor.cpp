#include "WBassXEditor.h"

using namespace juce;
using namespace wavy::flat;

namespace
{
// design grid (measured on the reference, reference/wbassx)
const Rectangle<int> kGraph (3, 38, 534, 243), kBottom (3, 285, 534, 107), kMix (541, 38, 134, 354), kOut (679, 38, 118, 339);
const Rectangle<int> kClip (718, 84, 46, 4);
constexpr float kGraphDbTop = 118.0f, kPxPerDb = 40.0f / 6.0f;            // 0 dB at y 118, 6 dB per 40 px
constexpr float kMeterTop = 119.0f, kMeterBottom = 283.0f;                 // 0 .. -40 dB
const float kTicks[] = { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -100.0f };

float graphY (float db) { return kGraphDbTop - db * kPxPerDb; }
}

WBassXEditor::WBassXEditor (WBassXProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wbassx::mode), "WBassX", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Flat),
      proc (p),
      input (make<VFader> (kWhite)), origBass (make<VFader> (kBlue)), harmonics (make<VFader> (kOrange)),
      ratio (make<Knob> ("Ratio")), response (make<Knob> ("Response")), decay (make<Knob> ("Decay")),
      freq (make<HSlider>()), slope (make<SlopeBox>()), lasso (make<LassoOverlay>())
{
    setAuxLook (std::make_unique<FlatLook>());
    // the window still resizes from its corner, but the grip is not drawn over the badge
    for (auto* c : getChildren())
        if (dynamic_cast<ResizableCornerComponent*> (c) != nullptr) c->setAlpha (0.0f);
    setFactoryPresets (wbassx::factoryPresets());
    setKeepOnPresetLoad ({ p.apvts.getParameter (wbassx::input) });     // presets leave Input alone, as the reference

    auto gainFmt = [] (double v) { return gainText ((float) v); };
    for (auto* f : { &input, &origBass, &harmonics }) f->format = gainFmt;
    attachSlider (input, wbassx::input, { 553, 90, 31, 250 }, 0.0);
    attachSlider (origBass, wbassx::origBass, { 592, 90, 31, 250 }, 0.0);
    attachSlider (harmonics, wbassx::harmonics, { 630, 90, 31, 250 }, -100.0);
    for (auto* f : { &input, &origBass, &harmonics }) applyFormat (*f, f->format);
    // measured on MaxxBass: Input's wheel and slow Ctrl steps go 1.6 times as far here (its range is 2/3 of the
    // reference's scale)
    input.wheelScale = 1.6; input.feel.linCtrlSlow *= 1.6f;

    ratio.format = [] (double v) { return String (v, 2) + ":1"; };
    response.format = [] (double v) { return String (v, 1); };
    decay.format = [] (double v) { return String (v, 1); };
    attachSlider (ratio, wbassx::ratio, { 106, 305, 54, 84 }, 1.0);
    attachSlider (response, wbassx::response, { 172, 305, 54, 84 }, 20.0);
    attachSlider (decay, wbassx::decay, { 418, 305, 54, 84 }, -15.0);
    for (auto* k : { &ratio, &response }) applyFormat (*k, k->format);
    applyFormat (decay, decay.format, true);         // a typed "15" means -15 (the reference's PositiveTextEntryToNegative)

    freq.format = [] (double v) { return String (roundToInt (v)) + " Hz"; };
    for (float f : { 32.0f, 64.0f, 128.0f, 256.0f }) freq.tickPositions.push_back (std::log2 (f / 32.0f) / 3.0f);
    freq.x0 = 7.0f; freq.x1 = 271.0f; freq.y = 16.0f;
    freq.textArea = { 284, 10, 50, 13 };
    attachSlider (freq, wbassx::freq, { 86, 286, 344, 26 }, 80.0);    // wide enough for the entry box (x 380 ... 429)
    freq.wheelScale = 1.2;                                                 // measured on MaxxBass's Freq

    for (auto* f : { &input, &origBass, &harmonics }) f->selection = &selection;
    for (auto* k : { &ratio, &response, &decay }) k->selection = &selection;
    freq.selection = &selection;

    slope.addItemList ({ "6 dB/oct", "12 dB/oct", "24 dB/oct" }, 1);
    slope.selection = &selection;
    attachCombo (slope, wbassx::highpass, { 340, 325, 57, 28 });              // box at 344, 329 (49 x 20)

    auto& mon = make<RadioColumn> (*p.apvts.getParameter (wbassx::monitor), &p.undo,
                                       StringArray { "Audio", "Harmonics", "Original Bass" },
                                       std::vector<Colour> { kWhite, kOrange, kBlue });
    mon.selection = &selection;
    addToContent (mon, { 708, 303, 69, 62 });
    monitor = &mon;
    setUpGroups();
    addToContent (lasso, { 0, 0, W, H });                                     // on top of everything

    // the graph follows every control
    for (auto* s : std::initializer_list<Slider*> { &origBass, &harmonics, &decay, &freq })
        s->onValueChange = [this] { content().repaint (kGraph); };
    slope.onChange = [this] { content().repaint (kGraph); };
}

float WBassXEditor::value (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

void WBassXEditor::tick()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001, dt = lastTick > 0.0 ? now - lastTick : 0.0;
    lastTick = now;
    const float l = proc.outLDb.take(), r = proc.outRDb.take();
    outL = barL.update (l, dt); outR = barR.update (r, dt);
    holdL = jmax (holdL, l); holdR = jmax (holdR, r);
    clip = clip || proc.clipped.exchange (false);
    if (const int b = proc.blocks.load(); b != lastBlocks) { lastBlocks = b; lastBlockMs = Time::getMillisecondCounterHiRes(); }
    content().repaint (kOut);
}

void WBassXEditor::setUpGroups()
{
    // the reference's control order (GUIXML ControlOrder); Freq does not take part in groups. Input's 0..1 covers only
    // the lower 2/3 of the reference's scale (it stops at 0 dB)
    groupable = { { &input, wbassx::input, 2.0f / 3.0f }, { &origBass, wbassx::origBass, 1.0f }, { &ratio, wbassx::ratio, 1.0f },
                  { &harmonics, wbassx::harmonics, 1.0f }, { &decay, wbassx::decay, 1.0f }, { &response, wbassx::response, 1.0f },
                  { monitor, wbassx::monitor, 1.0f }, { &slope, wbassx::highpass, 1.0f } };
    input.feel.referenceScale = 2.0f / 3.0f;
    auto paramOf = [this] (Component* c) -> RangedAudioParameter*
    {
        for (auto& m : groupable) if (m.c == c) return apvts.getParameter (m.id);
        return nullptr;
    };
    selection.gangBegin = [this, paramOf] (Component* src)
    {
        gangStart.clear(); gangTotal = 0.0f;
        for (auto* c : selection.members)
            if (c != src)
                if (auto* p = paramOf (c)) { p->beginChangeGesture(); gangStart[c] = p->getValue(); }
    };
    selection.gangEnd = [this, paramOf] (Component* src)
    {
        for (auto* c : selection.members) if (c != src) if (auto* p = paramOf (c)) p->endChangeGesture();
    };
    selection.gangMove = [this, paramOf] (Component* src, float step)
    {
        gangTotal += step;           // applied from each member's start value, so the parameters' rounding does not add up
        for (auto& m : groupable)
        {
            if (m.c == src || ! selection.contains (m.c)) continue;
            auto* p = paramOf (m.c);
            if (p == nullptr) continue;
            if (auto* choice = dynamic_cast<AudioParameterChoice*> (p))          // one setting per mouse move
            {
                const int n = choice->choices.size(), i = jlimit (0, n - 1, choice->getIndex() + (step > 0.0f ? 1 : -1));
                p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
            }
            else
                p->setValueNotifyingHost (jlimit (0.0f, 1.0f, gangStart[m.c] + gangTotal / m.referenceScale));
        }
    };
}

void WBassXEditor::contentMouseDown (const MouseEvent& e)
{
    // as in the reference: a click on the meters, readouts or the clip light clears the peaks and the clip light
    if (Rectangle<int> (700, 80, 80, 210).contains (e.getPosition()))
    {
        holdL = holdR = -100.0f; clip = false;
        content().repaint (kOut);
    }
    // a click on the background clears the selection and starts a lasso
    selection.clear();
    lassoStart = e.getPosition();
    lassoing = true;
}

void WBassXEditor::contentMouseDrag (const MouseEvent& e)
{
    if (! lassoing) return;
    const auto r = Rectangle<int> (lassoStart, e.getPosition());
    lasso.repaint (lasso.rect.expanded (2));
    lasso.rect = r;
    lasso.repaint (r.expanded (2));
    std::vector<Component*> group;
    for (auto& m : groupable)
        if (m.c->getBounds().intersects (r)) group.push_back (m.c);
    selection.setGroup (std::move (group));
}

void WBassXEditor::contentMouseUp (const MouseEvent&)
{
    if (! lassoing) return;
    lassoing = false;
    lasso.repaint (lasso.rect.expanded (2));
    lasso.rect = {};
}

void WBassXEditor::paintGraph (Graphics& g)
{
    const auto area = kGraph.toFloat();
    // background: deep blue on the left (original bass) to deep brown on the right (harmonics)
    ColourGradient bg (Colour (15, 28, 40), area.getX(), area.getY(), Colour (24, 15, 11), area.getRight(), area.getY(), false);
    bg.addColour (0.5, Colour (21, 22, 26));
    g.setGradientFill (bg);
    g.fillRect (area);
    ColourGradient shade (Colour (30, 50, 70).withAlpha (0.45f), area.getX(), area.getBottom(),
                          Colours::transparentBlack, area.getX() + area.getWidth() * 0.55f, area.getY() + area.getHeight() * 0.35f, false);
    g.setGradientFill (shade);
    g.fillRect (area);

    Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (kGraph);

    // dB grid labels and the 0 dB line
    g.setColour (Colour (80, 82, 88));
    g.fillRect (21.0f, kGraphDbTop, area.getRight() - 21.0f, 1.0f);
    g.setFont (font (10.0f));
    g.setColour (Colour (150, 158, 168));
    const std::pair<float, const char*> labels[] = { { 6.0f, "+6" }, { 0.0f, "0" }, { -6.0f, "-6" }, { -12.0f, "-12" }, { -18.0f, "-18" } };
    for (auto& [db, t] : labels)
        g.drawText (t, Rectangle<float> (4.0f, graphY (db) - 6.0f, 14.0f, 12.0f), Justification::centredRight);
    g.setColour (Colour (110, 125, 140).withAlpha (0.85f));
    g.fillRect (20.0f, graphY (6.0f), 1.0f, 275.0f - graphY (6.0f) + 1.0f);
    for (float db : { 6.0f, -12.0f, -18.0f }) g.fillRect (21.0f, graphY (db), 7.0f, 1.0f);
    g.fillRect (21.0f, 275.0f, 7.0f, 1.0f);

    // bands
    const float F = value (wbassx::freq), orig = value (wbassx::origBass), harm = value (wbassx::harmonics);
    const float dec = value (wbassx::decay);
    const int hp = roundToInt (value (wbassx::highpass));
    std::vector<Point<float>> blue, orange;
    for (float x = area.getX(); x <= area.getRight() + 1.0f; x += 2.0f)
    {
        const float f = 16.0f * std::pow (2.0f, (x - 4.0f) / 88.0f);
        if (orig > -99.0f)
            blue.push_back ({ x, graphY (orig - 10.0f * std::log10 (1.0f + std::pow (f / F, 6.0f))) });
        if (harm > -99.0f)
        {
            float db = harm;
            // low side as the reference draws it: 2nd order at 20 Hz (6 dB/oct), 2nd / 3rd order at Freq (12 / 24)
            if (hp == 0)      db -= 10.0f * std::log10 (1.0f + std::pow (20.0f / f, 4.0f));
            else if (hp == 1) db -= 10.0f * std::log10 (1.0f + std::pow (F / f, 4.0f));
            else              db -= 10.0f * std::log10 (1.0f + std::pow (F / f, 6.0f));
            // top of the harmonic series, fitted to the reference's graph (about 5.5 x Freq at Decay -15)
            const float fu = F * std::exp (1.72f + 0.031f * (dec + 15.0f));
            db -= 10.0f * std::log10 (1.0f + std::pow (f / fu, 4.7f));
            orange.push_back ({ x, graphY (db) });
        }
    }
    drawBand (g, orange, area.getBottom(), kOrange);
    drawBand (g, blue, area.getBottom(), kBlue);

    // frequency labels
    g.setFont (font (10.0f));
    g.setColour (Colour (200, 204, 210));
    const std::pair<float, const char*> fl[] = { { 46.0f, "16" }, { 92.0f, "32" }, { 180.0f, "64" }, { 268.0f, "128" }, { 356.0f, "256" } };
    for (auto& [x, t] : fl) g.drawText (t, Rectangle<float> (x - 15.0f, 269.0f, 30.0f, 12.0f), Justification::centred);

    g.setFont (font (12.0f));
    auto glowText = [&g] (const String& t, Rectangle<int> r, Colour c, Justification j)
    {
        g.setColour (c.withAlpha (0.055f));                                  // the reference's soft halo
        for (auto [dx, dy] : { std::pair<int, int> { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 } })
            g.drawText (t, r.translated (dx, dy), j);
        g.setColour (c);
        g.drawText (t, r, j);
    };
    glowText ("Original Bass", Rectangle<int> (37, 49, 120, 16), kBlue, Justification::centredLeft);
    glowText ("Harmonics", Rectangle<int> (374, 49, 120, 16), kOrange, Justification::centredRight);
}

void WBassXEditor::paintMeters (Graphics& g)
{
    const bool mono = isMono();
    const float meterH = kMeterBottom - kMeterTop;
    // bar top as measured on the reference (tools/MeterCapture, -6 / -20 dBFS bursts): 3.79 px per dB, -40 dB at y 274.7;
    // below -40 dB, while audio runs, a dot stays at the bottom (y 279 ... 283); with no audio at all, nothing
    const bool running = Time::getMillisecondCounterHiRes() - lastBlockMs < 500.0;
    auto top = [running] (float db) { return db > -40.0f ? 274.7f - 3.79f * (db + 40.0f) : running ? 278.6f : kMeterBottom; };
    g.setFont (font (12.0f));
    g.setColour (kValue);
    auto peakText = [] (float db) { return db <= -99.95f ? String ("-Inf") : String (db, 1); };
    if (mono)
    {
        drawMeterBarTo (g, { 715.0f, kMeterTop, 6.0f, meterH }, top (outL));
        g.setColour (kValue);
        g.drawText (peakText (holdL), Rectangle<int> (698, 100, 40, 15), Justification::centred);
    }
    else
    {
        drawMeterBarTo (g, { 715.0f, kMeterTop, 6.0f, meterH }, top (outL));
        drawMeterBarTo (g, { 761.0f, kMeterTop, 6.0f, meterH }, top (outR));
        g.setColour (kValue);
        g.drawText (peakText (holdL), Rectangle<int> (698, 100, 40, 15), Justification::centred);
        g.drawText (peakText (holdR), Rectangle<int> (744, 100, 40, 15), Justification::centred);
    }
    // scale between the bars (to the right of the bar in mono)
    const float sx = mono ? 750.0f : 741.0f;
    g.setFont (font (10.0f));
    for (int i = 0; i <= 5; ++i)
    {
        const float y = kMeterTop + meterH * (float) i / 5.0f;
        g.setColour (Colour (100, 101, 104));
        g.fillRect (sx - 14.0f, y, 6.0f, 1.0f);
        g.fillRect (sx + 8.0f, y, 6.0f, 1.0f);
        g.setColour (Colour (160, 160, 163));
        g.drawText (String (i * 8), Rectangle<float> (sx - 9.0f, y - 6.0f, 18.0f, 12.0f), Justification::centred);
    }
    g.setColour (clip ? Colour (235, 45, 45) : Colour (69, 40, 42));
    g.fillRect (kClip);
}

void WBassXEditor::paintContent (Graphics& g)
{
    g.fillAll (kGap);
    // frames: graph, bottom controls, mix and output panels
    g.setColour (kFrame);
    g.drawRect (kGraph.expanded (1), 1);
    g.drawRect (kBottom.expanded (1), 1);
    g.drawRect (kMix.expanded (1), 1);
    g.drawRect (kOut.expanded (1), 1);
    g.setColour (kPanel);
    g.fillRect (kBottom); g.fillRect (kMix); g.fillRect (kOut);
    g.setColour (Colour (34, 36, 39));
    g.fillRect (0, 36, W, 1);

    paintGraph (g);

    // bottom section labels and connecting lines
    g.setColour (kLabel);
    g.setFont (font (12.5f));
    g.drawText ("Dynamics", Rectangle<int> (20, 331, 75, 16), Justification::centredRight);
    g.drawText ("Harmonics", Rectangle<int> (240, 331, 80, 16), Justification::centredRight);
    g.setFont (font (12.0f));
    g.drawText ("Filter", Rectangle<int> (341, 356, 51, 16), Justification::centred);
    g.setColour (Colour (96, 97, 97));                              // measured: the same grey as the Filter box border
    for (auto [x0, x1] : { std::pair<int, int> { 98, 113 }, { 156, 179 }, { 323, 340 }, { 397, 425 } })
        g.fillRect (x0, 339, x1 - x0, 1);

    // mix panel: labels, fader scale ticks
    g.setColour (kLabel);
    g.setFont (font (12.0f));                                               // measured: centres 566 / 603 / 640
    g.drawText ("Input", Rectangle<int> (546, 57, 40, 15), Justification::centred);
    g.drawText ("Original", Rectangle<int> (581, 57, 50, 15), Justification::centred);
    g.drawText ("Bass", Rectangle<int> (582, 69, 50, 15), Justification::centred);
    g.setFont (font (11.0f));
    g.drawText ("Harmonic", Rectangle<int> (623, 57, 52, 15), Justification::centred);   // a little right, clear of "Original"
    g.setFont (font (12.0f));
    g.drawText ("Bass", Rectangle<int> (620, 69, 52, 15), Justification::centred);
    g.setColour (kTick);
    for (float db : kTicks)
    {
        const float y = std::round (117.0f + (1.0f - gainNormFromDb (db)) * 165.0f);
        g.fillRect (580.0f, y, 16.0f, 1.5f);
        g.fillRect (619.0f, y, 16.0f, 1.5f);
    }

    // output panel
    g.setColour (kLabel);
    g.setFont (font (12.5f));
    g.drawText ("Output", Rectangle<int> (700, 56, 80, 15), Justification::centred);
    paintMeters (g);

    // product badge, bottom right, drawn like the reference's: the output panel stops above it, a dark 1 px gap runs
    // along its top and its slanted left edge (cut into the corner of the mix panel), flat blue fading into orange,
    // spaced dark letters
    const Colour gap (21, 23, 26);
    Path cut;                                                       // gap + badge area, cut out of the mix panel corner
    cut.startNewSubPath (655.5f, 393.0f); cut.lineTo (674.0f, 377.5f); cut.lineTo (799.0f, 377.5f); cut.lineTo (799.0f, 393.0f);
    cut.closeSubPath();
    g.setColour (gap);
    g.fillPath (cut);
    g.setColour (Colour (54, 56, 59));                              // the mix panel's frame follows the cut
    g.drawLine (654.5f, 392.0f, 673.5f, 376.5f, 1.0f);
    Path badge;
    badge.startNewSubPath (659.0f, 393.0f); badge.lineTo (676.0f, 379.0f); badge.lineTo (799.0f, 379.0f); badge.lineTo (799.0f, 393.0f);
    badge.closeSubPath();
    ColourGradient bg (Colour (80, 142, 213), 713.0f, 0.0f, Colour (213, 140, 106), 743.0f, 0.0f, false);
    g.setGradientFill (bg);
    g.fillPath (badge);
    g.setColour (Colours::black.withAlpha (0.35f));                 // the reference's darker top row
    g.fillRect (676.0f, 379.0f, 123.0f, 1.0f);
    g.setColour (Colour (18, 20, 24));
    g.setFont (font (13.5f, true));
    const String name ("WBassX");
    const float pitch = 12.4f, x0 = 739.0f - pitch * (float) (name.length() - 1) * 0.5f;
    for (int i = 0; i < name.length(); ++i)
        g.drawText (String::charToString (name[i]), Rectangle<float> (x0 + pitch * (float) i - 7.0f, 379.0f, 14.0f, 14.0f), Justification::centred);
}
