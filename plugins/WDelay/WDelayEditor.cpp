#include "WDelayEditor.h"
#include "WavyLogo.h"
#include "WavyWheel.h"
#include "WDelayTapData.h"
#include "WDelayMeterData.h"

using namespace juce;
using namespace wavy::hybrid;

namespace
{
// design grid (the reference's GUIXML and screenshots, reference/hdelay): knob centres fitted on the bodies' lit edges
const Point<float> kDelay (204.63f, 101.63f), kFeedback (516.03f, 101.51f), kDepth (181.42f, 223.15f), kRate (286.16f, 222.61f),
    kHiPass (407.16f, 222.61f), kLoPass (542.25f, 222.56f), kMix (698.69f, 62.07f), kOutput (698.63f, 142.0f),
    kAnalog (698.47f, 226.89f), kLink (473.75f, 220.25f);
const Rectangle<int> kDisplay (290, 64, 145, 78);         // the window 292 .. 430 x 66 .. 139 and its bevel
const Rectangle<float> kMeterFrame (598.0f, 67.0f, 50.0f, 198.0f);      // the housing 600 .. 646 x 69 .. 262 and around it
const Point<float> kClipLamp (621.3f, 48.6f);
const Rectangle<float> kTapPad (24.0f, 59.0f, 93.0f, 95.0f);
const Colour kAmberSeg (250, 176, 10);

float rad (float deg) { return deg * MathConstants<float>::pi / 180.0f; }

// the knobs' drag law (measured on the reference's display and Link wheel, as on its knobs): 0.001 of the range per px
// on mouse steps up to 2 px, 0.002 from 3 px, a quarter of either with Ctrl
float dragRate (float stepPx) { return (std::abs (stepPx) <= 2.0f ? 0.001f : 0.002f) * (wavy::ctrlHeldNow() ? 0.25f : 1.0f); }
float wheelNotches (const MouseWheelDetails& w)
{
    return (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY) * (w.isReversed ? -1.0f : 1.0f) / (60.0f / 256.0f);
}
float refToJuce (float deg) { return rad (deg + 180.0f); }

// the meter's scale (fitted on the reference's ticks): y = 83.70 + 157.82 (1 - 10^(-dB / 40)), within 0.3 px
float meterY (float db) { return 83.70f + 157.82f * (1.0f - std::pow (10.0f, -jmax (0.0f, -db) / 40.0f)); }

// Ping Pong's start side (fitted on the reference, tools/fit_hdelay_display.py ppbox): an amber box (255, 180, 0) with
// corners of 2.1, leaning 0.04 px right per px up; its letter in straight strokes, a 1.27 px black core in a 1.65 px
// stroke at 0.53 (the strokes stop short of each other, as on a segment display). Unselected, the reference's dark
// rim: a black line down its left, darker rows above and below; selected, a yellow line round it at half strength.
void drawStartBox (Graphics& g, bool right, bool selected)
{
    const float lean = 0.04f;
    const auto shear = AffineTransform (1.0f, -lean, lean * 123.0f, 0.0f, 1.0f, 0.0f);
    if (selected)
    {
        Path ring;
        ring.addRoundedRectangle (390.72f, 94.5f, 22.6f, 28.0f, 2.6f);
        g.setColour (Colour (237, 213, 4).withAlpha (0.5f));
        g.strokePath (ring, PathStrokeType (1.0f), shear);
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.27f));
        g.fillRect (Rectangle<float> (393.5f, 94.0f, 19.5f, 1.0f));
        g.fillRect (Rectangle<float> (392.0f, 122.0f, 20.5f, 1.0f));
        g.setColour (Colours::black);
        g.fillRect (Rectangle<float> (390.0f, 95.0f, 1.0f, 21.0f));
        g.fillRect (Rectangle<float> (391.0f, 95.0f, 1.0f, 1.0f));
    }
    Path box;
    box.addRoundedRectangle (391.22f, 95.0f, 21.6f, 27.0f, 2.1f);
    g.setColour (Colour (255, 180, 0));
    g.fillPath (box, shear);
    static const float strokesL[][4] = { { 400.63f, 102.05f, 400.14f, 107.07f }, { 399.75f, 110.39f, 399.34f, 115.58f },
                                         { 401.20f, 116.16f, 406.52f, 116.14f } };
    static const float strokesR[][4] = { { 400.03f, 101.56f, 405.14f, 101.58f }, { 399.57f, 103.70f, 399.02f, 107.54f },
                                         { 407.32f, 101.97f, 407.01f, 107.24f }, { 400.71f, 108.82f, 405.41f, 108.77f },
                                         { 398.78f, 110.25f, 398.32f, 115.52f }, { 401.07f, 111.01f, 405.57f, 116.13f } };
    Path ink;
    auto add = [&ink] (const float (*st)[4], int n)
    {
        for (int i = 0; i < n; ++i) { ink.startNewSubPath (st[i][0], st[i][1]); ink.lineTo (st[i][2], st[i][3]); }
    };
    if (right) add (strokesR, 6); else add (strokesL, 3);
    g.setColour (Colours::black.withAlpha (0.53f));
    g.strokePath (ink, PathStrokeType (1.65f, PathStrokeType::mitered, PathStrokeType::square));
    g.setColour (Colours::black);
    g.strokePath (ink, PathStrokeType (1.27f, PathStrokeType::mitered, PathStrokeType::square));
}
}

WDelayEditor::WDelayEditor (WDelayProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wdelay::mode), "WDelay", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Hybrid, { "Stereo", "Mono", "M>S" }),
      proc (p),
      delay (make<Knob> (Knob::Grand, 39.0f, 321.0f)), delayNote (make<Knob> (Knob::Grand, 39.0f, 321.0f)),
      delayR (make<Knob> (Knob::Grand, 39.0f, 321.0f)), delayNoteR (make<Knob> (Knob::Grand, 39.0f, 321.0f)),
      feedback (make<Knob> (Knob::Grand, 39.0f, 321.0f)),
      depth (make<Knob> (Knob::Knurled, 39.0f, 321.0f)), rate (make<Knob> (Knob::Knurled, 39.0f, 321.0f)),
      hipass (make<Knob> (Knob::Knurled, 39.0f, 321.0f)), lopass (make<Knob> (Knob::Knurled, 39.0f, 321.0f)),
      mix (make<Knob> (Knob::Mini, 39.0f, 321.0f)), output (make<Knob> (Knob::Mini, 39.0f, 321.0f)),
      analog (make<Knob> (Knob::Mini, 39.0f, 321.0f)),
      display (make<Display> (*this)), tap (make<TapPad> (*this)), link (make<LinkWheel> (*this)), meter (make<Meter>()),
      rMark (make<RMark> (*this))
{
    setAuxLook (std::make_unique<HybridLook>());
    for (auto* c : getChildren())
        if (dynamic_cast<ResizableCornerComponent*> (c) != nullptr) c->setAlpha (0.0f);
    setFactoryPresets (wdelay::factoryPresets());
    setKeepOnPresetLoad ({ p.apvts.getParameter (wdelay::mix) });        // presets leave Mix alone, as the reference

    auto place = [this] (Knob& k, const char* id, Rectangle<int> r, Point<float> c, double reset, std::function<String (double)> fmt)
    {
        // the component covers the knob, its shadow and the value box (shown on the knob while it is set, as the
        // reference); clicks outside the knob fall through (hitTest)
        const float e = k.paintExtent();
        const auto drawn = Rectangle<float> (c.x - e, c.y - e, 2.0f * e, 2.0f * e).getSmallestIntegerContainer();
        const auto box = Rectangle<float> (37.0f, 12.0f).withCentre (c.translated (0.0f, 2.5f)).getSmallestIntegerContainer();
        const auto b = r.getUnion (drawn).getUnion (box);
        k.centre = c - b.getPosition().toFloat();
        k.valueArea = box - b.getPosition();
        k.format = std::move (fmt); k.selection = &selection;
        attachSlider (k, id, b, reset);
        if (k.format) wavy::flat::applyFormat (k, k.format, false);
    };
    auto noteText = [] (double v) { return wdelay::noteNames()[jlimit (0, 19, roundToInt (v))]; };
    place (delay, wdelay::delay, { 130, 27, 152, 154 }, kDelay, 375.0, [] (double v) { return String (roundToInt (v)); });
    place (delayR, wdelay::delayR, { 130, 27, 152, 154 }, kDelay, 375.0, [] (double v) { return String (roundToInt (v)); });
    place (delayNote, wdelay::delayNote, { 130, 27, 152, 154 }, kDelay, 11.0, {});
    place (delayNoteR, wdelay::delayNoteR, { 130, 27, 152, 154 }, kDelay, 11.0, {});
    for (auto* k : { &delayNote, &delayNoteR })
    {
        k->textFromValueFunction = noteText;
        k->steps = 20;
        const float px[4] = { 30.0f, 20.0f, 120.0f, 200.0f / 3.0f };     // as H-Comp's note knob (to be measured)
        std::copy (px, px + 4, k->feel.stepPx);
    }
    place (feedback, wdelay::feedback, { 442, 27, 152, 154 }, kFeedback, 60.0, [] (double v) { return String (roundToInt (v)); });
    place (depth, wdelay::depth, { 134, 175, 96, 106 }, kDepth, 0.0, [] (double v) { return String (roundToInt (v)); });
    place (rate, wdelay::rate, { 239, 175, 96, 106 }, kRate, 0.1, [] (double v) { return wdelay::rateText ((float) v).upToFirstOccurrenceOf (" ", false, false); });
    place (hipass, wdelay::hipass, { 360, 175, 96, 106 }, kHiPass, 19.0, [] (double v) { return wdelay::freqText ((float) v, true); });
    place (lopass, wdelay::lopass, { 495, 175, 96, 106 }, kLoPass, 20138.77, [] (double v) { return wdelay::freqText ((float) v, false); });
    place (mix, wdelay::mix, { 663, 26, 73, 76 }, kMix, 100.0, [] (double v) { return String (roundToInt (v)); });
    place (output, wdelay::output, { 663, 106, 73, 76 }, kOutput, 0.0, [] (double v) { return String (v, 1); });
    place (analog, wdelay::analog, { 663, 191, 73, 76 }, kAnalog, 2.0, {});
    analog.steps = 5;
    for (auto& px : analog.feel.stepPx) px = 50.0f;           // as H-Comp's Analog (to be measured)

    // Phase L, Ping Pong, Dual, Phase R in one frame (the reference's rectangles: 23, 62, 34, 22 px); the mono
    // component shows a single phase button
    auto* um = &p.undo;
    auto par = [&p] (const char* id) { return p.apvts.getParameter (id); };
    // (the H-Delay look: frames 2 px, faces of the measured widths; the component holds the frame, 4 px around it and
    // the shadow 9 px under it)
    const String slash (CharPointer_UTF8 ("\xc3\x98")), slashSmall (CharPointer_UTF8 ("\xc3\xb8"));     // "Ø", "ø"
    auto around = [] (Rectangle<int> frame) { return frame.withTrimmedLeft (-4).withTrimmedTop (-4).withTrimmedRight (-4).withTrimmedBottom (-9); };
    stereoRow = &make<ToggleRow> (std::vector<ToggleRow::Item> { { par (wdelay::phaseL), slash + "L", 20.0f },
                                                                 { par (wdelay::pingPong), "PING PONG", 59.0f },
                                                                 { par (wdelay::dual), "DUAL", 34.0f },
                                                                 { par (wdelay::phaseR), slash + "R", 20.0f } }, um, ButtonLook::hDelay);
    addToContent (*stereoRow, around ({ 290, 38, 143, 22 }));
    monoRow = &make<ToggleRow> (std::vector<ToggleRow::Item> { { par (wdelay::phaseL), slashSmall, 20.0f } }, um, ButtonLook::hDelay);
    addToContent (*monoRow, around ({ 290, 38, 24, 22 }));
    syncGroup = &make<ButtonGroup> (*par (wdelay::sync), um, StringArray { "BPM", "HOST", "MS" }, false, false, ButtonLook::hDelay);
    addToContent (*syncGroup, around ({ 292, 146, 141, 21 }));
    lofiButton = &make<ButtonGroup> (*par (wdelay::lofi), um, StringArray { "LoFi" }, false, true, ButtonLook::hDelay);
    addToContent (*lofiButton, around ({ 35, 188, 70, 22 }));
    // one control is selected at a time (the reference): a knob (its ring), a button group or a button (a yellow line),
    // the tap pad
    for (auto* r : { stereoRow, monoRow }) r->onClick = [this] (Component* c) { selectOnly (c); };
    for (auto* b : { syncGroup, lofiButton }) b->onClick = [this] (Component* c) { selectOnly (c); };

    addToContent (display, kDisplay);
    addToContent (tap, kTapPad.expanded (6.0f).getSmallestIntegerContainer());
    const auto linkBox = Rectangle<float> (40.0f, 44.0f).withCentre (kLink.translated (0.0f, -4.0f)).getSmallestIntegerContainer();
    link.centre = kLink - linkBox.getPosition().toFloat();
    addToContent (link, linkBox);
    addToContent (meter, kMeterFrame.getSmallestIntegerContainer());
    addToContent (rMark, { 233, 168, 14, 13 });
    updateDelayKnobs();
}

float WDelayEditor::value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void WDelayEditor::setValue (const char* id, float v)
{
    auto* p = proc.apvts.getParameter (id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (v));
    p->endChangeGesture();
}

double WDelayEditor::tempo() const
{
    if (roundToInt (value (wdelay::sync)) == 0) return value (wdelay::bpm);
    const double h = proc.hostBpm.load();
    return h > 0.0 ? h : 120.0;
}

// the Delay knob shows ms or note values (Sync), and in Dual the side chosen on the display
void WDelayEditor::updateDelayKnobs()
{
    const int s = roundToInt (value (wdelay::sync)), m = modeIndex();
    const bool dualOn = value (wdelay::dual) > 0.5f;
    const bool dual = dualOn && m != 1, ping = value (wdelay::pingPong) > 0.5f && m != 1;
    // as the reference: the right side starts from the left's time when Dual is switched on
    if (lastDualOn == 0 && dualOn)
    {
        setValue (wdelay::delayR, value (wdelay::delay));
        setValue (wdelay::delayNoteR, value (wdelay::delayNote));
    }
    lastDualOn = (int) dualOn;
    // the right side's knob while Dual's layout shows (Ping Pong's takes over the display and shows the left)
    const bool right = rightSide && dual && ! ping;
    if (s != lastSync && lastSync >= 0)
    {
        // as the reference: switching between ms and note values keeps the time, as near as the new unit allows
        const double t = tempo();
        if (s == 2)
        {
            setValue (wdelay::delay, (float) (wdelay::noteBeats (roundToInt (value (wdelay::delayNote))) * 60000.0 / t));
            setValue (wdelay::delayR, (float) (wdelay::noteBeats (roundToInt (value (wdelay::delayNoteR))) * 60000.0 / t));
        }
        else if (lastSync == 2)
        {
            setValue (wdelay::delayNote, (float) wdelay::nearestNote (value (wdelay::delay), t));
            setValue (wdelay::delayNoteR, (float) wdelay::nearestNote (value (wdelay::delayR), t));
        }
    }
    if (s == lastSync && m == lastMode && (int) dual == lastDual && (int) ping == lastPing && right == lastRight) return;
    lastSync = s; lastMode = m; lastDual = (int) dual; lastPing = (int) ping; lastRight = right;
    delay.setVisible (s == 2 && ! right);
    delayR.setVisible (s == 2 && right);
    delayNote.setVisible (s != 2 && ! right);
    delayNoteR.setVisible (s != 2 && right);
    rMark.setVisible (right);
    stereoRow->setVisible (m != 1);
    monoRow->setVisible (m == 1);
    selection.clear();
    content().repaint();
}

// the selection outline on one button control (or none); a knob's selection or the tap pad's go
void WDelayEditor::selectOnly (Component* c)
{
    selection.clear();
    tap.deselect();
    display.selectBox (false);
    syncGroup->setOutlined (c == syncGroup);
    lofiButton->setOutlined (c == lofiButton);
    if (c != stereoRow) stereoRow->setOutlinedItem (-1);
    if (c != monoRow) monoRow->setOutlinedItem (-1);
}

void WDelayEditor::tick()
{
    updateDelayKnobs();
    // as the reference: a note value whose time at the tempo passes 3500 ms is not taken (2 bars at 120 BPM: it stops at
    // 1 bar); the longest one that fits instead
    for (auto* id : { wdelay::delayNote, wdelay::delayNoteR })
    {
        int n = roundToInt (value (id));
        const int n0 = n;
        while (n > 0 && wdelay::noteBeats (n) * 60000.0 / tempo() > 3500.0) --n;
        if (n != n0) setValue (id, (float) n);
    }
    if (selection.primary != nullptr && (lastPrimary != selection.primary))      // a knob was clicked: the others go
    {
        lastPrimary = selection.primary;
        tap.deselect();
        display.selectBox (false);
        syncGroup->setOutlined (false); lofiButton->setOutlined (false);
        stereoRow->setOutlinedItem (-1); monoRow->setOutlinedItem (-1);
    }
    if (selection.primary == nullptr) lastPrimary = nullptr;
    display.repaint();
    const double now = Time::getMillisecondCounterHiRes() * 1.0e-3;
    const double dt = lastTick > 0.0 ? jlimit (0.0, 0.1, now - lastTick) : 1.0 / 60.0;
    lastTick = now;
    const float l = proc.outLDb.take(), r = proc.outRDb.take();
    meter.set (meterL.update (l, dt), meterR.update (r, dt), modeIndex() != 1);
    // the clip light: lit at 0 dBFS (the reference's ClipLevel 0.999999), held a moment
    clipAge = jmax (l, r) >= -8.7e-6f ? 0.0 : clipAge + dt;
    const double lit = clipAge < 1.0 ? 1.0 : 0.0;
    if (lit != clipLevel)
    {
        clipLevel = lit;
        content().repaint (Rectangle<float> (24.0f, 24.0f).withCentre (kClipLamp).toNearestInt());
    }
}

//==============================================================================
// measured on the reference: a black box 11 x 10 px after DELAY, a half-dark shadow along its left and under it; the R
// cut out of the box in the panel's colour (its stem 2.7 px, a counter of 0.85 px in the bowl, another under it, the
// leg leaning right)
void WDelayEditor::RMark::paint (Graphics& g)
{
    g.addTransform (AffineTransform::translation ((float) -getX(), (float) -getY()));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRect (Rectangle<float> (234.0f, 169.0f, 1.0f, 10.0f));
    g.fillRect (Rectangle<float> (235.0f, 179.0f, 11.0f, 1.0f));
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRect (Rectangle<float> (234.0f, 179.0f, 1.0f, 1.0f));
    g.setColour (Colours::black);
    g.fillRect (Rectangle<float> (235.0f, 169.0f, 11.0f, 10.0f));
    Path r;
    r.addRectangle (237.0f, 171.0f, 2.7f, 4.0f);                  // the stem, narrower under the bowl
    r.addRectangle (237.0f, 175.0f, 2.4f, 2.5f);
    r.addRectangle (237.0f, 171.0f, 5.5f, 1.0f);                  // the top
    r.addRoundedRectangle (240.55f, 171.0f, 3.3f, 4.0f, 1.5f);    // the bowl's right side
    r.addRectangle (237.0f, 174.0f, 5.0f, 1.0f);                  // the middle
    r.startNewSubPath (240.4f, 175.0f);                           // the leg
    r.lineTo (243.2f, 175.0f); r.lineTo (244.35f, 177.5f); r.lineTo (241.9f, 177.5f);
    r.closeSubPath();
    g.reduceClipRegion (r);
    drawPanel (g, { 0, 19, W, H - 19 }, ed.panelCache, PanelLook::hDelay);
}

//==============================================================================
void WDelayEditor::Display::paint (Graphics& g)
{
    // measured: a window sunk in the panel — the panel darkened along its left side (x 291: 0.53) and lit along its right
    // and bottom edges (x 431, 432 and y 140); a 1 px black rim (292 .. 430 x 66 .. 139, two rows at the top), a 1 px
    // dark ring inside, a lit lip under the top (171, 147, 104); the glass's grey (26 .. 97) as a 6 px grid of medians
    // over the reference's captures (the glyphs masked out): a smoky cloud at the top left fading to an even dark grey,
    // crossed by fine streaks that fall to the right (ours, of the same angle and depth)
    g.addTransform (AffineTransform::translation ((float) -kDisplay.getX(), (float) -kDisplay.getY()));
    const Rectangle<float> win (292.0f, 66.0f, 139.0f, 74.0f);
    g.setColour (Colours::black.withAlpha (0.47f));
    g.fillRect (Rectangle<float> (291.0f, 69.0f, 1.0f, 67.0f));
    g.setColour (Colour (218, 136, 72).withAlpha (0.55f));
    g.fillRect (Rectangle<float> (431.0f, 70.0f, 1.0f, 66.0f));
    g.setColour (Colour (218, 136, 72));
    g.fillRect (Rectangle<float> (432.0f, 71.0f, 0.8f, 64.0f));
    g.setColour (Colour (130, 91, 57));
    g.fillRect (Rectangle<float> (296.0f, 140.0f, 131.0f, 1.0f));
    g.setColour (Colours::black);
    g.fillRoundedRectangle (win, 5.0f);
    g.setColour (Colour (16, 16, 17));
    g.fillRoundedRectangle (win.reduced (1.0f).withTrimmedTop (1.0f), 4.0f);
    const float scale = jlimit (0.5f, 4.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 4.0f) / 4.0f);
    const Rectangle<float> pane (294.0f, 68.0f, 135.0f, 70.0f);
    if (glass.isNull() || glassScale != scale)
    {
        static const uint8 grid[12][23] = {
            { 63, 65, 69, 73, 84, 88, 88, 92, 97, 86, 81, 83, 82, 79, 76, 74, 70, 68, 66, 64, 60, 57, 56 },
            { 49, 50, 52, 56, 59, 69, 77, 82, 83, 79, 74, 67, 62, 60, 57, 53, 51, 50, 49, 48, 46, 45, 45 },
            { 38, 38, 39, 42, 45, 47, 51, 55, 61, 64, 61, 55, 51, 44, 44, 42, 40, 38, 36, 37, 36, 35, 35 },
            { 32, 32, 34, 34, 34, 35, 37, 36, 38, 41, 51, 48, 42, 36, 34, 32, 31, 31, 30, 30, 30, 30, 30 },
            { 29, 30, 30, 31, 32, 33, 33, 34, 34, 36, 40, 42, 43, 38, 34, 32, 30, 29, 29, 28, 27, 27, 27 },
            { 30, 30, 30, 31, 32, 34, 34, 34, 34, 33, 34, 35, 36, 35, 35, 33, 31, 29, 28, 27, 27, 27, 27 },
            { 29, 29, 30, 31, 31, 32, 32, 33, 34, 34, 34, 34, 34, 34, 34, 33, 31, 29, 28, 27, 27, 27, 27 },
            { 29, 29, 29, 29, 30, 30, 30, 30, 30, 31, 32, 32, 32, 33, 34, 34, 35, 33, 30, 28, 27, 26, 26 },
            { 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 32, 32, 33, 31, 30, 27, 26, 26 },
            { 28, 29, 29, 29, 30, 30, 30, 30, 29, 29, 30, 30, 30, 29, 28, 28, 29, 30, 30, 29, 27, 26, 26 },
            { 27, 27, 28, 29, 28, 29, 29, 30, 30, 30, 30, 29, 29, 28, 27, 27, 27, 27, 27, 27, 26, 26, 26 },
            { 27, 27, 28, 28, 29, 29, 28, 28, 29, 30, 30, 30, 30, 30, 28, 27, 27, 26, 27, 26, 26, 26, 28 } };
        glassScale = scale;
        const int w = (int) std::ceil (pane.getWidth() * scale), h = (int) std::ceil (pane.getHeight() * scale);
        glass = Image (Image::RGB, w, h, false);
        Image::BitmapData bd (glass, Image::BitmapData::writeOnly);
        Random grain (4321);
        float phase[6], amp[6];
        for (int k = 0; k < 6; ++k) { phase[k] = grain.nextFloat() * MathConstants<float>::twoPi; amp[k] = 0.5f + grain.nextFloat(); }
        for (int py = 0; py < h; ++py)
            for (int px = 0; px < w; ++px)
            {
                const float x = pane.getX() + ((float) px + 0.5f) / scale, y = pane.getY() + ((float) py + 0.5f) / scale;   // design
                float v;
                if (y < 69.0f) v = 171.0f;
                else if (y < 70.0f) v = 147.0f;
                else
                {
                    const float gx = jlimit (0.0f, 22.0f, (x - 294.0f) / 6.0f - 0.5f), gy = jlimit (0.0f, 11.0f, (y - 70.0f) / 6.0f - 0.5f);
                    const int ix = jmin (21, (int) gx), iy = jmin (10, (int) gy);
                    const float fx = gx - (float) ix, fy = gy - (float) iy;
                    v = (float) grid[iy][ix] * (1.0f - fx) * (1.0f - fy) + (float) grid[iy][ix + 1] * fx * (1.0f - fy)
                      + (float) grid[iy + 1][ix] * (1.0f - fx) * fy + (float) grid[iy + 1][ix + 1] * fx * fy;
                    // the streaks: thin waves along a line falling about 17 degrees to the right, deepest in the cloud
                    const float u = (y - 70.0f) - (x - 294.0f) * 0.30f, amount = jmax (0.0f, v - 28.0f) * 0.16f;
                    float st = 0.0f;
                    for (int k = 0; k < 6; ++k) st += amp[k] * std::sin (u * (0.9f + 0.35f * (float) k) + phase[k] + 0.04f * x * (float) (k % 3));
                    v += amount * st / 3.0f + (grain.nextFloat() - 0.5f) * 3.0f;
                }
                const auto u = (uint8) jlimit (0.0f, 255.0f, v);
                bd.setPixelColour (px, py, Colour (u, u, u));
            }
    }
    {
        g.saveState();
        Path clip;
        clip.addRoundedRectangle (pane, 3.0f);
        g.reduceClipRegion (clip);
        g.drawImageTransformed (glass, AffineTransform::scale (1.0f / scale).translated (pane.getX(), pane.getY()));
        g.restoreState();
    }
    // the characters (fitted on the reference, tools/fit_hdelay_display.py; design coordinates, each text from its first
    // cell's bottom-left x and its top y): the fonts, and where the reference puts each piece
    static const SegFont big { 12.85f, 22.65f, 2.45f, 2.26f, 1.56f, 0.175f, 17.05f, true },   // the tempo and the time
        noteF { 9.85f, 16.0f, 2.0f, 1.45f, 1.0f, 0.19f, 12.0f },                          // a note's figures and T / D
        dashF { 9.93f, 16.0f, 1.85f, 0.88f, 1.1f, 0.14f, 13.6f },                         // the fraction's bar, a dash per digit
        barF { 8.0f, 13.44f, 1.89f, 1.28f, 0.88f, 0.20f, 10.25f },                         // "BAR" after a 1
        label { 5.26f, 8.69f, 1.18f, 0.66f, 0.63f, 0.21f, 6.8f },                         // "BPM"
        mid { 10.4f, 17.24f, 1.95f, 1.54f, 0.73f, 0.15f, 14.0f },                         // Ping Pong's time
        dualMs { 9.88f, 17.05f, 1.85f, 0.72f, 0.79f, 0.10f, 14.02f },                     // Dual: the two times
        dualTempo { 10.03f, 17.01f, 2.01f, 1.02f, 0.83f, 0.17f, 13.89f },
        dualLabel { 5.26f, 8.68f, 0.99f, 0.6f, 0.42f, 0.18f, 6.42f },
        side { 6.1f, 9.4f, 1.1f, 0.6f, 0.4f, 0.17f, 7.0f },                                // Dual's "L" and "R"
        dualNote { 6.8f, 11.2f, 1.3f, 0.8f, 0.8f, 0.21f, 8.5f },
        dualBar { 5.59f, 8.72f, 1.23f, 0.51f, 0.5f, 0.17f, 6.48f };
    g.setColour (kAmberSeg);
    const int s = roundToInt (ed.value (wdelay::sync)), m = ed.modeIndex();
    const bool ping = ed.value (wdelay::pingPong) > 0.5f && m != 1, dual = ed.value (wdelay::dual) > 0.5f && m != 1 && ! ping;
    const String tempoText = String (roundToInt (s == 0 ? (double) ed.value (wdelay::bpm) : ed.tempo()));
    // digits right-aligned in a field whose first cell starts at x (the reference's 3-digit tempo, 4-digit time)
    auto field = [&] (const SegFont& f, float x, float y, const String& t, int cells)
    {
        drawSegmentText (g, f, x + f.pitch * (float) (cells - t.length()), y, t);
    };
    // a note as the reference prints it: "1" over a dash per digit over the denominator, then T or D; a bar as "1BAR"
    // (positions fitted on each of the reference's notes: its figures are not evenly spaced)
    auto note = [&] (int n, float dx)
    {
        const String t = wdelay::noteNames()[jlimit (0, 19, n)];
        if (t.endsWith ("B"))
        {
            drawSegmentText (g, SegFont { 10.92f, 17.02f, 1.89f, 1.28f, 0.88f, 0.20f, 12.0f }, 298.67f + dx, 96.13f, t.substring (0, 1));
            drawSegmentText (g, barF, 313.85f + dx, 99.88f, "BAR");
            return;
        }
        const String den = t.fromFirstOccurrenceOf ("/", false, false).retainCharacters ("0123456789");
        const bool two = den.length() > 1;
        drawSegmentText (g, noteF, (two ? 308.0f : 302.0f) + dx, 80.95f, "1");
        drawSegmentText (g, dashF, 302.7f + dx, 97.0f, two ? "--" : "-");
        struct Pos { const char* d; float x0, x1; };
        static const Pos pos[] = { { "2", 300.7f, 0 }, { "4", 299.04f, 0 }, { "8", 300.42f, 0 }, { "16", 299.6f, 310.94f },
                                   { "32", 301.85f, 314.28f }, { "64", 301.94f, 312.54f } };
        for (auto& q : pos)
            if (den == q.d)
            {
                drawSegmentText (g, noteF, q.x0 + dx, 112.0f, den.substring (0, 1));
                if (two) drawSegmentText (g, noteF, q.x1 + dx, 112.0f, den.substring (1, 2));
            }
        if (t.endsWithChar ('T') || t.endsWithChar ('D'))
        {
            // T and D fitted on their own: T's strokes run nearer the corners
            static const SegFont tF { 9.84f, 16.03f, 1.98f, 0.64f, 0.58f, 0.19f, 12.0f }, dF { 9.7f, 16.03f, 2.0f, 1.24f, 0.95f, 0.20f, 12.0f };
            drawSegmentText (g, t.endsWithChar ('T') ? tF : dF, (two ? 331.7f : 321.7f) + dx, 96.9f, t.getLastCharacters (1));
        }
    };
    // Dual's smaller notes, at a side's offset (measured on 1/8D and 1 bar)
    auto smallNote = [&] (int n, float dx)
    {
        const String t = wdelay::noteNames()[jlimit (0, 19, n)];
        if (t.endsWith ("B"))
        {
            drawSegmentText (g, dualNote, 307.65f + dx, 95.8f, t.substring (0, 1));
            drawSegmentText (g, dualBar, 302.76f + dx, 117.8f, "BAR");
            return;
        }
        const String den = t.fromFirstOccurrenceOf ("/", false, false).retainCharacters ("0123456789");
        const float shift = den.length() > 1 ? 4.0f : 0.0f;
        drawSegmentText (g, dualNote, 303.74f + shift + dx, 96.42f, "1");
        g.fillRect (Rectangle<float> (303.0f + dx, 111.0f, 6.0f, 1.4f));        // the bar: two short strokes
        g.fillRect (Rectangle<float> (311.0f + dx, 111.0f, 6.0f, 1.4f));
        drawSegmentText (g, dualNote, 304.23f + dx - (den.length() > 1 ? 4.0f : 0.0f), 115.66f, den);
        if (t.endsWithChar ('T') || t.endsWithChar ('D'))
            drawSegmentText (g, dualNote, 319.33f + dx + shift, 106.32f, t.getLastCharacters (1));
    };
    auto msText = [] (float v) { return String (roundToInt (v)).paddedLeft ('0', 4); };
    if (ping)
    {
        // the left half of Dual's layout (the reference draws the same pixels: the time, or the note and the tempo) and
        // the start side's box
        if (s == 2) drawSegmentText (g, dualMs, 299.31f, 101.89f, msText (ed.value (wdelay::delay)));
        else
        {
            smallNote (roundToInt (ed.value (wdelay::delayNote)), 0.0f);
            field (dualTempo, 338.85f, 95.95f, tempoText, 3);
            drawSegmentText (g, dualLabel, 359.93f, 118.25f, "BPM");
        }
        drawStartBox (g, ed.value (wdelay::pingPongStart) > 0.5f, boxSelected);
        return;
    }
    if (dual)
    {
        drawSegmentText (g, side, 309.32f, 76.95f, "L");
        drawSegmentText (g, side, 405.71f, 76.75f, "R");
        if (s == 2)
        {
            drawSegmentText (g, dualMs, 299.31f, 101.89f, msText (ed.value (wdelay::delay)));
            drawSegmentText (g, dualMs, 369.39f, 101.92f, msText (ed.value (wdelay::delayR)));
            g.setColour (kAmberSeg.withAlpha (0.37f));                                   // the divider between them
            g.fillRect (Rectangle<float> (360.0f, 80.0f, 2.0f, 50.0f));
        }
        else
        {
            smallNote (roundToInt (ed.value (wdelay::delayNote)), 0.0f);
            smallNote (roundToInt (ed.value (wdelay::delayNoteR)), 94.0f);
            field (dualTempo, 338.85f, 95.95f, tempoText, 3);
            drawSegmentText (g, dualLabel, 359.93f, 118.25f, "BPM");
        }
        return;
    }
    if (s == 2)
    {
        drawSegmentText (g, big, 327.16f, 92.15f, msText (ed.value (wdelay::delay)));
        return;
    }
    note (roundToInt (ed.value (wdelay::delayNote)), 0.0f);
    field (big, 366.71f, 93.4f, tempoText, 3);
    drawSegmentText (g, label, 398.65f, 119.76f, "BPM");
}

void WDelayEditor::Display::mouseDown (const MouseEvent& e)
{
    ed.selectOnly (nullptr);
    const auto h = hit (e.position + kDisplay.getPosition().toFloat());          // design coordinates
    if (h.box)
    {
        // the start side flips at each click, and the box is the selected control
        ed.setValue (wdelay::pingPongStart, ed.value (wdelay::pingPongStart) > 0.5f ? 0.0f : 1.0f);
        selectBox (true);
        return;
    }
    if (h.side >= 0)
    {
        ed.rightSide = h.side == 1;
        ed.updateDelayKnobs();
    }
    dragParam = h.param;
    noteDrag = h.note;
    stepAcc = 0.0f;
    if (dragParam == nullptr) return;
    lastY = e.position.y;
    e.source.enableUnboundedMouseMovement (true, false);
    dragParam->beginChangeGesture();
}

// what a click, a vertical drag or the wheel sets at a point of the display (design coordinates): the reference's
// control rectangles (its GUIXML; the first listed takes a point two share), checked with clicks and drags on it
WDelayEditor::Display::Hit WDelayEditor::Display::hit (Point<float> p) const
{
    const int s = roundToInt (ed.value (wdelay::sync)), m = ed.modeIndex();
    const bool ping = ed.value (wdelay::pingPong) > 0.5f && m != 1, dual = ed.value (wdelay::dual) > 0.5f && m != 1 && ! ping;
    auto par = [this] (const char* id) { return ed.proc.apvts.getParameter (id); };
    auto in = [p] (float x, float y, float w, float h) { return Rectangle<float> (x, y, w, h).contains (p); };
    Hit h;
    if (ping && in (390.0f, 94.0f, 26.0f, 29.0f)) { h.box = true; return h; }
    if (ping || dual)
    {
        // the left time or note, the right one (Dual), the tempo between them (BPM)
        if (s == 2)
        {
            if (in (293.0f, 100.0f, 66.0f, 22.0f)) { h.param = par (wdelay::delay); h.side = 0; }
            else if (dual && in (321.0f, 78.0f, 104.0f, 55.0f)) { h.param = par (wdelay::delayR); h.side = 1; }
        }
        else if (in (298.0f, 95.0f, 34.0f, 34.0f)) { h.param = par (wdelay::delayNote); h.side = 0; h.note = true; }
        else if (dual && in (392.0f, 95.0f, 34.0f, 34.0f)) { h.param = par (wdelay::delayNoteR); h.side = 1; h.note = true; }
        else if (s == 0 && in (338.0f, 94.0f, 42.0f, 21.0f)) h.param = par (wdelay::bpm);
        if (! dual) h.side = -1;
        return h;
    }
    if (s == 2) { if (in (294.0f, 62.0f, 137.0f, 74.0f)) h.param = par (wdelay::delay); }
    else if (in (300.0f, 80.0f, 45.0f, 49.0f)) { h.param = par (wdelay::delayNote); h.note = true; }
    else if (s == 0 && in (292.0f, 57.0f, 137.0f, 74.0f)) h.param = par (wdelay::bpm);
    return h;
}

void WDelayEditor::Display::mouseDrag (const MouseEvent& e)
{
    if (dragParam == nullptr) return;
    const float dy = e.position.y - lastY;
    lastY = e.position.y;
    if (noteDrag)
    {
        // a note value steps as on the Delay knob: 30 px per value on slow mouse steps (up to 2 px), 20 on quicker ones;
        // with Ctrl 120 and 66.7 (measured: 40 px moved the note one value, on the display as on the knob)
        const bool slow = std::abs (dy) <= 2.0f, fine = wavy::ctrlHeldNow();
        stepAcc -= dy / (fine ? (slow ? 120.0f : 200.0f / 3.0f) : (slow ? 30.0f : 20.0f));
        const int steps = (int) stepAcc;
        if (steps == 0) return;
        stepAcc -= (float) steps;
        const float v = jlimit (0.0f, 19.0f, (float) roundToInt (dragParam->convertFrom0to1 (dragParam->getValue())) + (float) steps);
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (v));
        return;
    }
    dragParam->setValueNotifyingHost (jlimit (0.0f, 1.0f, dragParam->getValue() - dy * dragRate (dy)));
}

void WDelayEditor::Display::mouseUp (const MouseEvent& e)
{
    if (dragParam == nullptr) return;
    e.source.enableUnboundedMouseMovement (false);
    dragParam->endChangeGesture();
    dragParam = nullptr;
}

void WDelayEditor::Display::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const auto h = hit (e.position + kDisplay.getPosition().toFloat());
    auto* p = h.param;
    if (p == nullptr || e.mods.isAnyMouseButtonDown()) return;
    p->beginChangeGesture();
    if (h.note)                                                     // a note value: one per notch
    {
        const int n = roundToInt (wheelNotches (w));
        const float v = jlimit (0.0f, 19.0f, (float) roundToInt (p->convertFrom0to1 (p->getValue())) + (float) n);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    else p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + wheelNotches (w) / 72.0f));
    p->endChangeGesture();
}

//==============================================================================
namespace
{
// depth in from a rounded rectangle's edge (positive inside)
float rrDepth (float x, float y, float x0, float y0, float x1, float y1, float r)
{
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f - r, hy = (y1 - y0) * 0.5f - r;
    const float qx = std::abs (x - cx) - hx, qy = std::abs (y - cy) - hy;
    return -(std::hypot (jmax (qx, 0.0f), jmax (qy, 0.0f)) + jmin (jmax (qx, qy), 0.0f) - r);
}

// the tap pad rendered from its fitted shading (WDelayTapData.h, tools/fit_hdelay_tap.py) with its cast shadow
// (measured: offset 0.24, 1.81 px, edge 1.2 px, 0.44 deep), premultiplied, for one state at one scale
Image renderTap (bool pressed, float scale, Rectangle<float> area)
{
    namespace td = wdelay::tapdata;
    const int16_t* c = pressed ? td::pressed : td::released;
    const int w = (int) std::ceil (area.getWidth() * scale), h = (int) std::ceil (area.getHeight() * scale);
    Image img (Image::ARGB, w, h, true);
    Image::BitmapData bd (img, Image::BitmapData::readWrite);
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < w; ++px)
        {
            const float x = area.getX() + ((float) px + 0.5f) / scale, y = area.getY() + ((float) py + 0.5f) / scale;
            const float d = rrDepth (x, y, td::x0, td::y0, td::x1, td::y1, td::radius);
            const float sd = -rrDepth (x - 0.24f, y - 1.81f, td::x0, td::y0, td::x1, td::y1, td::radius);
            const float shadow = 0.444f * 0.5f * (1.0f - std::tanh (sd / 1.2f));
            const float cover = jlimit (0.0f, 1.0f, d + 0.5f);
            float rgb[3] = { 0.0f, 0.0f, 0.0f };
            if (cover > 0.0f)
            {
                const float df = rrDepth (x, y, td::fx0, td::fy0, td::fx1, td::fy1, td::fradius);
                const bool face = df > 0.0f;
                const float bin = face ? 96.0f + df / td::step : jmin (d, 47.5f) / td::step;
                const int bi = jlimit (0, td::bins - 1, roundToInt (bin));
                const float X0 = face ? td::fx0 : td::x0, Y0 = face ? td::fy0 : td::y0, X1 = face ? td::fx1 : td::x1, Y1 = face ? td::fy1 : td::y1;
                const float dk[4] = { y - Y0, Y1 - y, x - X0, X1 - x };
                const float dmin = jmin (jmin (dk[0], dk[1]), jmin (dk[2], dk[3]));
                float wk[4], sum = 0.0f;
                const float tau = 0.8f + 0.3f * jmax (dmin, 0.0f);                       // softer deeper in
                for (int k = 0; k < 4; ++k) { wk[k] = std::exp (-(dk[k] - dmin) / tau); sum += wk[k]; }
                const float ux = (x - (X0 + X1) * 0.5f) / ((X1 - X0) * 0.5f), uy = (y - (Y0 + Y1) * 0.5f) / ((Y1 - Y0) * 0.5f);
                const float u[4] = { ux, ux, uy, uy };
                for (int ch = 0; ch < 3; ++ch)
                {
                    const int16_t* q = c + ((size_t) bi * 3 + (size_t) ch) * (size_t) td::terms;
                    float v = 0.0f;
                    for (int k = 0; k < 4; ++k) v += wk[k] / sum * ((float) q[k] + (float) q[k + 4] * u[k]);
                    rgb[ch] = jlimit (0.0f, 255.0f, v * 0.25f);
                }
            }
            // the pad over its shadow: alpha = the pad's cover, or the shadow's darkness outside it
            const float a = cover + (1.0f - cover) * shadow;
            auto u8 = [] (float v) { return (uint8) jlimit (0.0f, 255.0f, v + 0.5f); };
            *reinterpret_cast<PixelARGB*> (bd.getPixelPointer (px, py)) =
                PixelARGB (u8 (a * 255.0f), u8 (rgb[0] * cover), u8 (rgb[1] * cover), u8 (rgb[2] * cover));
        }
    return img;
}
}

void WDelayEditor::TapPad::paint (Graphics& g)
{
    // the pad as fitted on the reference, released or pressed (pressed: the top lip goes dark, the face sinks and its
    // lower edge catches the light); the maker's mark in grey on the face (ours in its place: 33 x 26 px, grey 100, 83
    // pressed); selected (after a click, until another control is): a yellow line just outside the pad
    const float scale = jlimit (0.5f, 4.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 4.0f) / 4.0f);
    const auto area = getBounds().toFloat();                                       // design coordinates
    if (imgScale != scale)
    {
        imgScale = scale;
        up = renderTap (false, scale, area);
        down = renderTap (true, scale, area);
    }
    g.drawImageTransformed (pressed ? down : up, AffineTransform::scale (1.0f / scale));
    const auto o = area.getPosition();
    wavy::drawMark (g, Rectangle<float> (53.0f, 93.0f, 33.5f, 26.5f) - o, false, false,
                    pressed ? Colour (83, 83, 83) : Colour (100, 100, 100), 1.4f);
    if (selected)
    {
        g.setColour (Colour (240, 215, 0).withAlpha (0.55f));
        g.drawRoundedRectangle (Rectangle<float> (23.5f, 58.5f, 95.0f, 97.0f) - o, 6.5f, 1.0f);
    }
}

void WDelayEditor::TapPad::mouseDown (const MouseEvent&)
{
    ed.selectOnly (nullptr);
    pressed = selected = true;
    repaint();
    const double now = Time::getMillisecondCounterHiRes();
    const double ms = now - lastTap;
    lastTap = now;
    if (ms < 1.0 || ms > 3500.0) return;                  // the first tap, or too long ago
    const int s = roundToInt (ed.value (wdelay::sync));
    const bool right = ed.rightSide;
    if (s == 2) ed.setValue (right ? wdelay::delayR : wdelay::delay, (float) ms);
    else if (s == 1) ed.setValue (right ? wdelay::delayNoteR : wdelay::delayNote, (float) wdelay::nearestNote (ms, ed.tempo()));
    else ed.setValue (wdelay::bpm, jlimit (30.0f, 300.0f, (float) (60000.0 / ms)));
}

//==============================================================================
void WDelayEditor::LinkWheel::paint (Graphics& g)
{
    // measured: a thin black ring of radius 19.2 about (473.8, 219.2); the wheel (8 square teeth, 16 px outside, 13.5 at
    // the roots) about (473.2, 218.3) with a soft shadow down and right; an arrowed arc above, its top at y 195
    const auto c = centre + Point<float> (0.05f, -1.0f), wc = centre + Point<float> (-0.55f, -1.9f);
    g.setColour (Colour (20, 14, 8));
    g.drawEllipse (c.x - 19.2f, c.y - 19.2f, 38.4f, 38.4f, 1.3f);
    Path arc;
    arc.addCentredArc (c.x, c.y + 10.0f, 34.0f, 34.0f, 0.0f, rad (-24.0f), rad (24.0f), true);
    g.strokePath (arc, PathStrokeType (1.1f));
    for (float side : { -1.0f, 1.0f })
    {
        const float a = rad (25.0f * side);
        const Point<float> tip (c.x + 34.0f * std::sin (a), c.y + 10.0f - 34.0f * std::cos (a));
        Path head;
        head.addTriangle (tip.translated (2.5f * side, 1.2f), tip.translated (-2.5f * side, -1.8f), tip.translated (-1.4f * side, 2.6f));
        g.fillPath (head);
    }
    for (int k = 4; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        const float r = 14.5f + 0.8f * (float) k;
        g.fillEllipse (wc.x + 3.5f - r, wc.y + 5.0f - r, 2.0f * r, 2.0f * r);
    }
    Path gear;
    const int teeth = 8;
    for (int i = 0; i < teeth; ++i)
    {
        // a tooth's flat top over 20 degrees, its sides falling over 4, the root between over 17
        const float a0 = turn + MathConstants<float>::twoPi * (float) i / (float) teeth;
        const float pts[4][2] = { { -12.0f, 13.5f }, { -9.5f, 16.0f }, { 9.5f, 16.0f }, { 12.0f, 13.5f } };
        for (auto& q : pts)
        {
            const float a = a0 + rad (q[0]);
            const Point<float> v (wc.x + q[1] * std::sin (a), wc.y - q[1] * std::cos (a));
            if (i == 0 && &q == &pts[0]) gear.startNewSubPath (v); else gear.lineTo (v);
        }
    }
    gear.closeSubPath();
    ColourGradient gg (Colour (78, 78, 80), wc.x - 8.0f, wc.y - 10.0f, Colour (26, 26, 27), wc.x + 9.0f, wc.y + 11.0f, false);
    g.setGradientFill (gg);
    g.fillPath (gear);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.strokePath (gear, PathStrokeType (0.8f));
    ColourGradient cap (Colour (66, 66, 68), wc.x - 5.0f, wc.y - 7.0f, Colour (36, 36, 37), wc.x + 6.0f, wc.y + 7.0f, false);
    g.setGradientFill (cap);
    g.fillEllipse (wc.x - 9.5f, wc.y - 9.5f, 19.0f, 19.0f);
}

void WDelayEditor::LinkWheel::mouseDown (const MouseEvent& e)
{
    lastY = e.position.y;
    e.source.enableUnboundedMouseMovement (true, false);
    for (auto* id : { wdelay::hipass, wdelay::lopass }) ed.proc.apvts.getParameter (id)->beginChangeGesture();
}

void WDelayEditor::LinkWheel::mouseDrag (const MouseEvent& e)
{
    const float dy = e.position.y - lastY;
    lastY = e.position.y;
    move (-dy * dragRate (dy));
}

void WDelayEditor::LinkWheel::mouseUp (const MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement (false);
    for (auto* id : { wdelay::hipass, wdelay::lopass }) ed.proc.apvts.getParameter (id)->endChangeGesture();
}

void WDelayEditor::LinkWheel::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    for (auto* id : { wdelay::hipass, wdelay::lopass }) ed.proc.apvts.getParameter (id)->beginChangeGesture();
    move (wheelNotches (w) / 72.0f);
    for (auto* id : { wdelay::hipass, wdelay::lopass }) ed.proc.apvts.getParameter (id)->endChangeGesture();
}

// both filters by the same amount of their (equal) ranges, as far as the one nearer its end can go
void WDelayEditor::LinkWheel::move (float d)
{
    auto* hp = ed.proc.apvts.getParameter (wdelay::hipass);
    auto* lp = ed.proc.apvts.getParameter (wdelay::lopass);
    const float a = hp->getValue(), b = lp->getValue();
    d = jlimit (-jmin (a, b), 1.0f - jmax (a, b), d);
    if (d == 0.0f) return;
    hp->setValueNotifyingHost (a + d);
    lp->setValueNotifyingHost (b + d);
    turn += d * 12.0f;
    repaint();
}

//==============================================================================
void WDelayEditor::Meter::set (float lDb, float rDb, bool stereo)
{
    if (std::abs (lDb - l) < 0.05f && std::abs (rDb - r) < 0.05f && stereo == two) return;
    l = lDb; r = rDb; two = stereo;
    repaint();
}

namespace
{
// the meter's housing, scale and print as one image (premultiplied), design area `area`, at one scale: the frame and the
// window from their fitted shading (WDelayMeterData.h, tools/fit_hdelay_meter.py)
Image renderMeterHousing (float scale, Rectangle<float> area)
{
    namespace md = wdelay::meterdata;
    const int w = (int) std::ceil (area.getWidth() * scale), h = (int) std::ceil (area.getHeight() * scale);
    Image img (Image::ARGB, w, h, true);
    {
        Image::BitmapData bd (img, Image::BitmapData::readWrite);
        const int ww = md::winW, wh = md::winH;
        for (int py = 0; py < h; ++py)
            for (int px = 0; px < w; ++px)
            {
                const float x = area.getX() + ((float) px + 0.5f) / scale, y = area.getY() + ((float) py + 0.5f) / scale;
                if (x < md::ox0 || x >= md::ox1 || y < md::oy0 || y >= md::oy1) continue;
                float rgb[3];
                if (x > md::wx0 && x < md::wx1 && y > md::wy0 && y < md::wy1)
                {
                    const int ix = jlimit (0, ww - 1, (int) (x - md::wx0)), iy = jlimit (0, wh - 1, (int) (y - md::wy0));
                    for (int ch = 0; ch < 3; ++ch)
                    {
                        float v = 0.0f;
                        for (int k = 0; k < md::rank; ++k)
                            v += (float) md::rows[(ch * md::rank + k) * wh + iy] / 16.0f * (float) md::cols[(ch * md::rank + k) * ww + ix] / md::colScale;
                        rgb[ch] = v;
                    }
                }
                else
                {
                    const float dk[4] = { y - md::oy0, md::oy1 - y, x - md::ox0, md::ox1 - x };
                    const float dmin = jmin (jmin (dk[0], dk[1]), jmin (dk[2], dk[3]));
                    float wk[4], sum = 0.0f;
                    for (int k = 0; k < 4; ++k) { wk[k] = std::exp (-(dk[k] - dmin) / 0.5f); sum += wk[k]; }
                    const float ux = (x - (md::ox0 + md::ox1) * 0.5f) / ((md::ox1 - md::ox0) * 0.5f), uy = (y - (md::oy0 + md::oy1) * 0.5f) / ((md::oy1 - md::oy0) * 0.5f);
                    const float u[4] = { ux, ux, uy, uy };
                    const int bi = jlimit (0, md::frameBins - 1, (int) dmin);
                    for (int ch = 0; ch < 3; ++ch)
                    {
                        const int16_t* q = md::frame + ((size_t) bi * 3 + (size_t) ch) * (size_t) md::frameTerms;
                        float v = 0.0f;
                        for (int k = 0; k < 4; ++k) v += wk[k] / sum * ((float) q[k] + (float) q[k + 4] * u[k]);
                        rgb[ch] = v / 16.0f;
                    }
                }
                auto u8 = [] (float v) { return (uint8) jlimit (0.0f, 255.0f, v + 0.5f); };
                *reinterpret_cast<PixelARGB*> (bd.getPixelPointer (px, py)) = PixelARGB (255, u8 (rgb[0]), u8 (rgb[1]), u8 (rgb[2]));
            }
    }
    // the scale (measured on the reference): ticks 7 px long from both sides of the window, about 2.5 px thick; the
    // numbers in a bold serif at their ink centres, a dark red (136, 12, 10) to 24, a dark olive below
    Graphics g (img);
    g.addTransform (AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    struct Mark { float db; const char* t; float cx, cy; bool red; };
    static const Mark marks[] = { { 0, "0", 622.5f, 83.5f, true }, { -3, "3", 623.0f, 107.5f, true }, { -6, "6", 622.5f, 129.5f, true },
                                  { -12, "12", 623.0f, 162.5f, true }, { -24, "24", 623.0f, 200.5f, true },
                                  { -36, "36", 622.5f, 220.5f, false }, { -48, "48", 622.5f, 232.0f, false } };
    const Font f = Font (FontOptions ("Times New Roman", 10.0f, Font::bold).withPointHeight (13.6f));
    for (auto m : marks)
    {
        const float y = meterY (m.db);
        g.setColour (Colour (66, 56, 38).withAlpha (0.85f));
        g.fillRect (Rectangle<float> (607.0f, y - 1.2f, 7.0f, 2.4f));
        g.fillRect (Rectangle<float> (632.0f, y - 1.2f, 7.0f, 2.4f));
        g.setColour (m.red ? Colour (136, 12, 10) : Colour (62, 50, 30));
        GlyphArrangement ga;
        ga.addLineOfText (f, m.t, 0.0f, 0.0f);
        Path pth;
        ga.createPath (pth);
        const auto pb = pth.getBounds();
        const auto at = AffineTransform::translation (m.cx - pb.getCentreX(), m.cy - pb.getCentreY());
        g.fillPath (pth, at);
        g.strokePath (pth, PathStrokeType (0.3f), at);                 // the reference's print is a little heavier
    }
    return img;
}
}

void WDelayEditor::Meter::paint (Graphics& g)
{
    const float scale = jlimit (0.5f, 4.0f, std::round (g.getInternalContext().getPhysicalPixelScaleFactor() * 4.0f) / 4.0f);
    if (housingScale != scale) { housingScale = scale; housing = renderMeterHousing (scale, kMeterFrame); }
    g.drawImageTransformed (housing, AffineTransform::scale (1.0f / scale));
    // a red triangle per channel pointing inward (measured at rest: 608 .. 618 x 239 .. 246, its point 1 px below the
    // scale's -inf; the body red to a darker red, a pink lip along its top edge, a thin shadow under it)
    g.addTransform (AffineTransform::translation (-kMeterFrame.getX(), -kMeterFrame.getY()));
    auto tri = [&] (float db, bool left)
    {
        const float y = (db <= -99.0f ? meterY (-200.0f) : meterY (db)) + 1.0f;
        const float xb = left ? 608.0f : 638.5f, xt = left ? 618.0f : 628.5f;
        Path p;
        p.addTriangle (xb, y - 3.6f, xt, y - 0.1f, xb, y + 3.6f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillPath (p, AffineTransform::translation (0.4f, 1.4f));
        ColourGradient body (Colour (200, 30, 30), 0.0f, y - 3.6f, Colour (110, 0, 0), 0.0f, y + 3.6f, false);
        body.addColour (0.45, Colour (172, 2, 2));
        g.setGradientFill (body);
        g.fillPath (p);
        g.setColour (Colour (214, 90, 90));
        g.drawLine (xb, y - 3.4f, xt - (left ? 1.0f : -1.0f), y - 0.4f, 1.0f);
    };
    tri (l, true);
    tri (two ? r : l, false);
}

//==============================================================================
void WDelayEditor::paintContent (Graphics& g)
{
    drawPanel (g, { 0, 19, W, H - 19 }, panelCache, PanelLook::hDelay);
    g.setColour (Colours::black);                                     // the panel's outline (measured: 1 px, left, right, bottom)
    g.fillRect (0, 19, 1, H - 19);
    g.fillRect (W - 1, 19, 1, H - 19);
    g.fillRect (0, H - 1, W, 1);

    // scales printed on the panel (fitted on the reference: 21 ticks of 14.1 degrees; the radii of each size)
    const float s0 = refToJuce (39.0f), s1 = refToJuce (321.0f);
    auto ticks = [&] (Point<float> c, int n, float rIn, float rOut, float thick)
    {
        Scale s; s.ticks = n; s.rIn = rIn; s.rOut = rOut; s.thick = thick; s.colour = Colours::black;
        drawScale (g, c, s0, s1, s);
    };
    ticks (kDelay, 21, 64.3f, 72.2f, 1.9f);
    ticks (kFeedback, 21, 64.3f, 72.2f, 1.9f);
    for (auto c : { kDepth, kRate, kHiPass, kLoPass }) ticks (c, 21, 33.5f, 38.7f, 1.8f);
    ticks (kMix, 21, 24.7f, 30.6f, 1.6f);
    ticks (kOutput, 21, 24.7f, 30.6f, 1.6f);
    ticks (kAnalog, 5, 23.9f, 31.3f, 1.7f);

    // printed text: each at the centre of its ink as measured on the reference
    g.setColour (Colours::black);                                    // measured: the print is black
    // (sizes and widths matched to the reference's ink: Arial Black labels, Arial Bold numbers)
    auto label = [&] (const char* t, float x, float y, float em, float hs) { printText (g, t, { x, y }, em, hs, true); };
    auto num = [&] (const char* t, float x, float y, float em = 9.4f) { printText (g, t, { x, y }, em, 1.1f, false); };
    label ("TAP", 68.5f, 164.0f, 11.5f, 1.12f);
    label ("DELAY", 207.5f, 174.5f, 10.75f, 1.25f); label ("FEEDBACK", 516.0f, 174.5f, 10.75f, 1.28f);
    label ("DEPTH", 181.0f, 265.0f, 9.75f, 1.2f); label ("RATE", 286.0f, 265.0f, 9.75f, 1.2f);
    label ("HiPASS", 407.5f, 265.0f, 9.75f, 1.12f); label ("LoPASS", 543.0f, 265.0f, 9.75f, 1.17f);
    label ("LINK", 474.5f, 247.0f, 7.2f, 1.25f);
    label ("MODULATION", 231.0f, 279.0f, 10.75f, 1.31f); label ("FILTERS", 477.0f, 279.5f, 10.75f, 1.35f);
    label ("DRY/WET", 700.5f, 97.0f, 8.6f, 1.32f); label ("OUTPUT", 700.5f, 177.0f, 8.6f, 1.3f); label ("ANALOG", 700.5f, 265.0f, 8.6f, 1.3f);
    if (roundToInt (value (wdelay::sync)) == 2) { num ("1", 147.5f, 162.5f); num ("3500", 258.0f, 162.5f); }
    else { num ("1/64T", 149.0f, 162.5f); num ("2Bar", 259.0f, 163.0f); }
    num ("0", 469.5f, 162.5f); num ("200", 568.5f, 162.5f);
    num ("0", 155.5f, 254.0f); num ("100", 212.0f, 254.0f);
    num ("0.1", 255.0f, 254.5f); num ("6K", 315.0f, 254.0f);
    num ("20Hz", 374.0f, 254.0f); num ("20kHz", 442.0f, 254.0f);
    num ("20Hz", 509.0f, 254.5f); num ("20kHz", 577.0f, 254.5f);
    num ("0", 676.0f, 89.0f, 8.6f); num ("100", 728.5f, 89.0f, 8.6f);
    num ("-18", 676.0f, 167.0f, 8.6f); num ("+18", 724.5f, 167.0f, 8.6f);
    num ("OFF", 667.0f, 253.5f, 8.0f); num ("1", 666.5f, 214.5f, 8.6f); num ("2", 699.0f, 190.5f, 8.6f);
    num ("3", 731.5f, 214.5f, 8.6f); num ("4", 721.7f, 253.3f, 8.6f);

    // the brackets under MODULATION and FILTERS, the lines from the filters to the Link wheel
    {
        auto bracket = [&] (float x0, float x1, float t0, float t1)
        {
            Path p;
            p.startNewSubPath (x0, 274.0f); p.lineTo (x0, 279.8f); p.lineTo (t0, 279.8f);
            p.startNewSubPath (t1, 279.8f); p.lineTo (x1, 279.8f); p.lineTo (x1, 274.0f);
            g.strokePath (p, PathStrokeType (1.6f));
        };
        bracket (163.5f, 298.5f, 175.0f, 287.0f);           // measured: the strokes end 2 px from the words
        bracket (427.5f, 525.5f, 442.0f, 511.0f);
        g.drawLine (441.0f, 221.0f, 452.5f, 221.0f, 1.3f);
        g.drawLine (490.5f, 221.0f, 509.0f, 221.0f, 1.3f);
    }

    // the clip light above the meter (measured on the reference's pixels): a cast shadow (0.46 of the panel, 3 px down
    // and right); a bezel to radius 12.3, bright only along its top (166, 170, 173), dark and bluish on the left, darker
    // on the right, pinkish below with a dull red arc from 4 to 7 o'clock; a black ring at 8.5; the dome to 7.8, dark
    // (15, 11, 11) at its edge and (72, 58, 58) a little below its middle; a small highlight on its upper left. Lit: red.
    {
        const auto c = kClipLamp;
        for (int k = 4; k >= 1; --k)
        {
            g.setColour (Colours::black.withAlpha (0.13f));
            const float r = 11.0f + 0.6f * (float) k;
            g.fillEllipse (c.x + 3.0f - r, c.y + 3.0f - r, 2.0f * r, 2.0f * r);
        }
        ColourGradient bz (Colour (60, 64, 70), c.x - 12.0f, c.y, Colour (34, 30, 33), c.x + 12.0f, c.y, false);
        g.setGradientFill (bz);
        g.fillEllipse (c.x - 12.3f, c.y - 12.3f, 24.6f, 24.6f);
        ColourGradient top (Colour (168, 170, 173), c.x, c.y - 12.0f, Colour (168, 170, 173).withAlpha (0.0f), c.x, c.y - 6.0f, false);
        g.setGradientFill (top);
        g.fillEllipse (c.x - 12.3f, c.y - 12.3f, 24.6f, 24.6f);
        ColourGradient low (Colour (94, 66, 69).withAlpha (0.0f), c.x, c.y + 5.0f, Colour (94, 66, 69), c.x, c.y + 11.0f, false);
        g.setGradientFill (low);
        g.fillEllipse (c.x - 12.3f, c.y - 12.3f, 24.6f, 24.6f);
        Path rim;
        rim.addCentredArc (c.x, c.y, 10.0f, 10.0f, 0.0f, rad (95.0f), rad (185.0f), true);
        g.setColour (Colour (128, 22, 26));
        g.strokePath (rim, PathStrokeType (1.2f));
        g.setColour (Colour (6, 5, 5));
        g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
        ColourGradient dome (clipLevel > 0.0 ? Colour (255, 70, 50) : Colour (72, 58, 58), c.x + 0.3f, c.y + 2.6f,
                             clipLevel > 0.0 ? Colour (120, 0, 0) : Colour (15, 11, 11), c.x + 0.3f, c.y + 2.6f + 9.5f, true);
        g.setGradientFill (dome);
        g.fillEllipse (c.x - 7.8f, c.y - 7.8f, 15.6f, 15.6f);
        g.setColour (Colour (205, 205, 210).withAlpha (0.9f));
        g.fillEllipse (c.x - 5.2f, c.y - 5.0f, 2.6f, 2.0f);
    }
    // the plug-in's name where the reference prints its own (measured: black, 18 px high; the word in Arial from x 42 to
    // 123, a little wide; our first letter in Arial Black, narrowed to the reference's first letter's place)
    {
        auto inkPath = [] (const Font& f, const String& t)
        {
            GlyphArrangement ga;
            ga.addLineOfText (f, t, 0.0f, 0.0f);
            Path p;
            ga.createPath (p);
            return p;
        };
        g.setColour (Colours::black);
        auto fit = [&] (Path p, Rectangle<float> box)
        {
            const auto b = p.getBounds();
            g.fillPath (p, AffineTransform::translation (-b.getX(), -b.getY()).scaled (box.getWidth() / b.getWidth(), box.getHeight() / b.getHeight())
                                                                             .translated (box.getX(), box.getY()));
        };
        fit (inkPath (Font (FontOptions ("Arial Black", 24.0f, Font::plain)), "W"), { 19.5f, 241.0f, 19.5f, 18.0f });
        fit (inkPath (Font (FontOptions ("Arial", 24.0f, Font::plain)), "DELAY"), { 42.0f, 241.0f, 81.0f, 18.0f });
    }
}
