#include "WDelayEditor.h"
#include "WavyLogo.h"
#include "WavyWheel.h"

using namespace juce;
using namespace wavy::hybrid;

namespace
{
// design grid (the reference's GUIXML and screenshots, reference/hdelay): knob centres fitted on the bodies' lit edges
const Point<float> kDelay (204.63f, 101.63f), kFeedback (516.03f, 101.51f), kDepth (181.42f, 223.15f), kRate (286.16f, 222.61f),
    kHiPass (407.16f, 222.61f), kLoPass (542.25f, 222.56f), kMix (698.69f, 62.07f), kOutput (698.63f, 142.0f),
    kAnalog (698.47f, 226.89f), kLink (471.36f, 219.5f);
const Rectangle<int> kDisplay (292, 66, 139, 72);
const Rectangle<float> kMeterFrame (600.0f, 69.0f, 45.0f, 192.0f);
const Point<float> kClipLamp (622.5f, 49.2f);
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

// the meter's scale (measured): the printed marks lie at y = 83.3 + 157.3 (1 - 10^(-dB / 40)), within 0.5 px
float meterY (float db) { return 83.3f + 157.3f * (1.0f - std::pow (10.0f, -jmax (0.0f, -db) / 40.0f)); }
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
      display (make<Display> (*this)), tap (make<TapPad> (*this)), link (make<LinkWheel> (*this)), meter (make<Meter>())
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
    stereoRow = &make<ToggleRow> (std::vector<ToggleRow::Item> { { par (wdelay::phaseL), String (CharPointer_UTF8 ("\xc3\x98")) + "L", 23.0f },
                                                                 { par (wdelay::pingPong), "PING PONG", 62.0f },
                                                                 { par (wdelay::dual), "DUAL", 34.0f },
                                                                 { par (wdelay::phaseR), String (CharPointer_UTF8 ("\xc3\x98")) + "R", 22.0f } }, um);
    addToContent (*stereoRow, Rectangle<int> (290, 38, 142, 21).expanded (4));
    monoRow = &make<ToggleRow> (std::vector<ToggleRow::Item> { { par (wdelay::phaseL), String (CharPointer_UTF8 ("\xc3\xb8")), 23.0f } }, um);   // "ø", as the reference
    addToContent (*monoRow, Rectangle<int> (290, 38, 24, 21).expanded (4));
    auto& sync = make<ButtonGroup> (*par (wdelay::sync), um, StringArray { "BPM", "HOST", "MS" }, false);
    addToContent (sync, Rectangle<int> (292, 146, 140, 21).expanded (4));
    auto& lofi = make<ButtonGroup> (*par (wdelay::lofi), um, StringArray { "LoFi" }, false, true);
    addToContent (lofi, Rectangle<int> (35, 188, 70, 22).expanded (4));

    addToContent (display, kDisplay);
    addToContent (tap, kTapPad.expanded (6.0f).getSmallestIntegerContainer());
    const auto linkBox = Rectangle<float> (40.0f, 44.0f).withCentre (kLink.translated (0.0f, -4.0f)).getSmallestIntegerContainer();
    link.centre = kLink - linkBox.getPosition().toFloat();
    addToContent (link, linkBox);
    addToContent (meter, kMeterFrame.getSmallestIntegerContainer());
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
    const bool dual = value (wdelay::dual) > 0.5f && m != 1, ping = value (wdelay::pingPong) > 0.5f && m != 1;
    if (! dual || ping) rightSide = false;
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
    if (s == lastSync && m == lastMode && (int) dual == lastDual && (int) ping == lastPing && rightSide == lastRight) return;
    lastSync = s; lastMode = m; lastDual = (int) dual; lastPing = (int) ping; lastRight = rightSide;
    delay.setVisible (s == 2 && ! rightSide);
    delayR.setVisible (s == 2 && rightSide);
    delayNote.setVisible (s != 2 && ! rightSide);
    delayNoteR.setVisible (s != 2 && rightSide);
    stereoRow->setVisible (m != 1);
    monoRow->setVisible (m == 1);
    selection.clear();
    content().repaint();
}

void WDelayEditor::tick()
{
    updateDelayKnobs();
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
void WDelayEditor::Display::paint (Graphics& g)
{
    // measured: a black rounded window (radius 5, a 1.5 px rim) over dark glass (30, 30, 30) with a light top edge and
    // a smoky streak that falls from upper left to the right; amber segment characters
    const auto b = getLocalBounds().toFloat();
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (b.translated (0.5f * (float) k, 1.0f * (float) k), 5.0f);
    }
    g.setColour (Colours::black);
    g.fillRoundedRectangle (b, 5.0f);
    const auto glass = b.reduced (1.5f);
    {
        Image img (Image::RGB, (int) glass.getWidth(), (int) glass.getHeight(), false);
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const float fy = (float) y, fx = (float) x;
                float v = 29.0f + 58.0f * std::exp (-fy / 5.0f);
                const float sx = 40.0f + 2.4f * fy;                               // the streak's centre moves right as it falls
                v += 38.0f * std::exp (-square ((fx - sx) / 18.0f)) * std::exp (-fy / 16.0f);
                if (y == 0) v = 165.0f;
                const auto u = (uint8) jlimit (0.0f, 255.0f, v);
                bd.setPixelColour (x, y, Colour (u, u, u));
            }
        g.saveState();
        Path clip;
        clip.addRoundedRectangle (glass, 4.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (img, (int) glass.getX(), (int) glass.getY());
        g.restoreState();
    }
    // design positions in local coordinates
    const Point<float> o ((float) -kDisplay.getX(), (float) -kDisplay.getY());
    auto at = [&] (float x, float y) { return Point<float> (x, y) + o; };
    g.setColour (kAmberSeg);
    const int s = roundToInt (ed.value (wdelay::sync)), m = ed.modeIndex();
    const bool ping = ed.value (wdelay::pingPong) > 0.5f && m != 1, dual = ed.value (wdelay::dual) > 0.5f && m != 1 && ! ping;
    const String bpmText = String (roundToInt (s == 0 ? (double) ed.value (wdelay::bpm) : ed.tempo()));
    // a note value as the reference prints it: the fraction stacked ("1" over a bar over "8"), then T or D; bars as "1B"
    auto note = [&] (int n, Point<float> topLeft, float k)
    {
        const String t = wdelay::noteNames()[jlimit (0, 19, n)];
        const float cw = 12.0f * k, ch = 16.0f * k, th = 2.0f * k;
        if (t.endsWith ("B"))
        {
            drawSegText (g, topLeft.translated (0.0f, 14.0f * k), t.substring (0, 1), cw, ch, 14.0f * k, th, 0.15f);
            drawSegText (g, topLeft.translated (16.0f * k, 22.0f * k), "B", cw * 0.8f, ch * 0.8f, 10.0f * k, th, 0.15f);
            return;
        }
        const String den = t.fromFirstOccurrenceOf ("/", false, false).retainCharacters ("0123456789");
        const String suffix = t.endsWithChar ('T') ? "T" : t.endsWithChar ('D') ? "D" : "";
        const float dw = (float) den.length() * 14.0f * k;
        drawSegText (g, topLeft.translated (dw * 0.5f - 7.0f * k + 2.0f * k, 0.0f), "1", cw, ch, 14.0f * k, th, 0.15f);
        g.fillRect (Rectangle<float> (topLeft.x + 2.0f * k, topLeft.y + 23.0f * k, dw - 4.0f * k, 2.5f * k));
        drawSegText (g, topLeft.translated (0.0f, 31.0f * k), den, cw, ch, 14.0f * k, th, 0.15f);
        if (suffix.isNotEmpty()) drawSegText (g, topLeft.translated (dw + 7.0f * k, 16.0f * k), suffix, cw, ch, 14.0f * k, th, 0.15f);
    };
    auto bigDigits = [&] (const String& t, float rightX, float y, float cw, float ch, float pitch, float th)
    {
        drawSegText (g, { rightX - pitch * (float) (t.length() - 1) - cw, y }, t, cw, ch, pitch, th, 0.15f);
    };
    auto msText = [] (float v) { return String (roundToInt (v)).paddedLeft ('0', 4); };
    if (ping)
    {
        // the time (ms or note) and the start side in an amber box
        if (s == 2) bigDigits (msText (ed.value (wdelay::delay)), 356.0f + o.x, 102.0f + o.y, 10.5f, 17.0f, 14.0f, 2.0f);
        else note (roundToInt (ed.value (wdelay::delayNote)), at (301.0f, 81.0f), 1.0f);
        const auto box = Rectangle<float> (392.0f, 95.0f, 22.0f, 27.0f) + o;
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (Colour (20, 16, 10));
        drawSegText (g, box.getPosition().translated (6.0f, 5.0f), ed.value (wdelay::pingPongStart) > 0.5f ? "R" : "L", 10.0f, 17.0f, 12.0f, 2.2f, 0.12f);
        return;
    }
    if (dual)
    {
        drawSegText (g, at (309.0f, 77.0f), "L", 6.0f, 9.0f, 7.0f, 1.2f, 0.15f);
        drawSegText (g, at (405.0f, 77.0f), "R", 6.0f, 9.0f, 7.0f, 1.2f, 0.15f);
        if (s == 2)
        {
            bigDigits (msText (ed.value (wdelay::delay)), 352.0f + o.x, 102.0f + o.y, 9.0f, 15.0f, 11.5f, 1.8f);
            bigDigits (msText (ed.value (wdelay::delayR)), 424.0f + o.x, 102.0f + o.y, 9.0f, 15.0f, 11.5f, 1.8f);
        }
        else
        {
            note (roundToInt (ed.value (wdelay::delayNote)), at (304.0f, 96.0f), 0.68f);
            note (roundToInt (ed.value (wdelay::delayNoteR)), at (398.0f, 96.0f), 0.68f);
            bigDigits (bpmText, 379.0f + o.x, 96.0f + o.y, 10.5f, 17.0f, 13.5f, 2.0f);
            drawSegText (g, at (360.0f, 118.0f), "BPM", 6.0f, 9.0f, 7.0f, 1.1f, 0.15f);
        }
        // the side the Delay knob sets: its letter underlined
        g.fillRect (Rectangle<float> (ed.rightSide ? 404.0f : 308.0f, 88.0f, 9.0f, 1.2f) + o);
        return;
    }
    if (s == 2)
    {
        bigDigits (msText (ed.value (wdelay::delay)), 393.0f + o.x, 92.0f + o.y, 13.5f, 23.0f, 17.5f, 2.6f);
        return;
    }
    note (roundToInt (ed.value (wdelay::delayNote)), at (301.0f, 81.0f), 1.0f);
    bigDigits (bpmText, 419.0f + o.x, 93.0f + o.y, 13.5f, 23.0f, 18.0f, 2.6f);
    drawSegText (g, at (399.0f, 120.0f), "BPM", 6.0f, 8.0f, 7.0f, 1.1f, 0.15f);
}

void WDelayEditor::Display::mouseDown (const MouseEvent& e)
{
    const int m = ed.modeIndex();
    const bool ping = ed.value (wdelay::pingPong) > 0.5f && m != 1, dual = ed.value (wdelay::dual) > 0.5f && m != 1 && ! ping;
    const auto p = e.position + kDisplay.getPosition().toFloat();         // design coordinates
    const int s = roundToInt (ed.value (wdelay::sync));
    if (ping && Rectangle<float> (392.0f, 95.0f, 22.0f, 27.0f).contains (p))
    {
        ed.setValue (wdelay::pingPongStart, ed.value (wdelay::pingPongStart) > 0.5f ? 0.0f : 1.0f);
        return;
    }
    if (dual) ed.rightSide = p.x > 380.0f;
    ed.updateDelayKnobs();
    dragParam = target (p);
    if (dragParam == nullptr) return;
    lastY = e.position.y;
    e.source.enableUnboundedMouseMovement (true, false);
    dragParam->beginChangeGesture();
}

// what a vertical drag or the wheel sets at a point of the display (design coordinates), as measured on the reference:
// the time in MS (the side's in Dual), the tempo over its digits in BPM; nothing over a note value
RangedAudioParameter* WDelayEditor::Display::target (Point<float> p) const
{
    const int s = roundToInt (ed.value (wdelay::sync)), m = ed.modeIndex();
    const bool ping = ed.value (wdelay::pingPong) > 0.5f && m != 1, dual = ed.value (wdelay::dual) > 0.5f && m != 1 && ! ping;
    if (s == 2) return ed.proc.apvts.getParameter (dual && p.x > 380.0f ? wdelay::delayR : wdelay::delay);
    if (s == 0 && (dual ? (p.x > 340.0f && p.x < 385.0f) : p.x > 360.0f)) return ed.proc.apvts.getParameter (wdelay::bpm);
    return nullptr;
}

void WDelayEditor::Display::mouseDrag (const MouseEvent& e)
{
    if (dragParam == nullptr) return;
    const float dy = e.position.y - lastY;
    lastY = e.position.y;
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
    auto* p = target (e.position + kDisplay.getPosition().toFloat());
    if (p == nullptr || e.mods.isAnyMouseButtonDown()) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + wheelNotches (w) / 72.0f));
    p->endChangeGesture();
}

//==============================================================================
void WDelayEditor::TapPad::paint (Graphics& g)
{
    // measured: a black rounded block (radius 6) with a lit top edge, a recessed square face (63 px) in a thin light
    // frame, and the maker's mark printed in grey on it; our mark in its place
    const auto b = kTapPad - getPosition().toFloat();
    for (int k = 5; k >= 1; --k)
    {
        g.setColour (Colours::black.withAlpha (0.08f));
        g.fillRoundedRectangle (b.translated (0.6f * (float) k, 1.2f * (float) k), 7.0f);
    }
    g.setColour (Colour (8, 8, 8));
    g.fillRoundedRectangle (b, 6.0f);
    ColourGradient body (Colour (64, 64, 64), b.getX(), b.getY() + 3.0f, Colour (36, 36, 36), b.getX(), b.getBottom() - 3.0f, false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (b.reduced (2.0f), 5.0f);
    g.setColour (Colour (122, 122, 122));
    g.drawLine (b.getX() + 7.0f, b.getY() + 3.0f, b.getRight() - 7.0f, b.getY() + 3.0f, 1.2f);
    const auto face = Rectangle<float> (63.0f, 63.0f).withCentre (b.getCentre().translated (0.0f, 0.5f));
    g.setColour (Colour (82, 82, 82));
    g.drawRoundedRectangle (face.expanded (2.5f), 5.0f, 1.0f);
    g.setColour (pressed ? Colour (30, 30, 30) : Colour (44, 44, 44));
    g.fillRoundedRectangle (face, 4.0f);
    g.setColour (Colour (20, 20, 20));
    g.drawRoundedRectangle (face, 4.0f, 1.0f);
    wavy::drawMark (g, Rectangle<float> (34.0f, 26.0f).withCentre (face.getCentre()), false, false, Colour (104, 104, 104), 1.4f);
}

void WDelayEditor::TapPad::mouseDown (const MouseEvent&)
{
    pressed = true;
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
    // measured: a black knurled wheel (9 teeth, 15 px outside, 12.5 at the roots) in a thin black ring of radius 19,
    // an arrowed arc above it
    const auto c = centre;
    g.setColour (Colour (20, 14, 8));
    g.drawEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f, 1.3f);
    Path arc;
    arc.addCentredArc (c.x, c.y + 6.0f, 26.0f, 26.0f, 0.0f, rad (-28.0f), rad (28.0f), true);
    g.strokePath (arc, PathStrokeType (1.1f));
    for (float side : { -1.0f, 1.0f })
    {
        const float a = rad (29.0f * side);
        const Point<float> tip (c.x + 26.0f * std::sin (a), c.y + 6.0f - 26.0f * std::cos (a));
        Path head;
        head.addTriangle (tip.translated (2.5f * side, 1.2f), tip.translated (-2.5f * side, -1.8f), tip.translated (-1.4f * side, 2.6f));
        g.fillPath (head);
    }
    DropShadow (Colours::black.withAlpha (0.45f), 5, { 3, 5 }).drawForRectangle (g, Rectangle<float> (26.0f, 26.0f).withCentre (c).toNearestInt());
    Path gear;
    const int teeth = 9;
    for (int i = 0; i < teeth * 4; ++i)
    {
        const float a = turn + MathConstants<float>::twoPi * ((float) i / (float) (teeth * 4));
        const float r = (i % 4 == 0 || i % 4 == 1) ? 15.0f : 12.5f;
        const Point<float> q (c.x + r * std::sin (a), c.y - r * std::cos (a));
        if (i == 0) gear.startNewSubPath (q); else gear.lineTo (q);
    }
    gear.closeSubPath();
    ColourGradient gg (Colour (78, 78, 80), c.x - 8.0f, c.y - 10.0f, Colour (26, 26, 27), c.x + 9.0f, c.y + 11.0f, false);
    g.setGradientFill (gg);
    g.fillPath (gear);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.strokePath (gear, PathStrokeType (0.8f));
    ColourGradient cap (Colour (66, 66, 68), c.x - 5.0f, c.y - 7.0f, Colour (36, 36, 37), c.x + 6.0f, c.y + 7.0f, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - 9.5f, c.y - 9.5f, 19.0f, 19.0f);
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

void WDelayEditor::Meter::paint (Graphics& g)
{
    // measured: a black outline around a dark brown frame, the cream window lit from the left (a bright band near its
    // left edge) darkening to olive and to near black at the bottom; ticks at both sides of each mark; the numbers
    // in a bold serif, red to 24 and dark below; a red triangle per channel pointing inward (left: L, right: R)
    const auto o = getPosition().toFloat();
    auto at = [&] (float x, float y) { return Point<float> (x, y) - o; };
    const auto frame = kMeterFrame - o;
    g.setColour (Colour (14, 8, 4));
    g.drawRect (frame, 1.0f);
    g.setColour (Colour (92, 42, 14));
    g.fillRect (frame.reduced (1.0f));
    g.setColour (Colour (52, 22, 6));
    g.drawRect (frame.reduced (4.0f), 1.0f);
    const auto win = Rectangle<float> (609.0f, 76.0f, 28.0f, 178.0f) - o;
    ColourGradient cream (Colour (214, 208, 176), 0.0f, win.getY(), Colour (32, 24, 10), 0.0f, win.getBottom(), false);
    cream.addColour (0.25, Colour (206, 198, 160));
    cream.addColour (0.60, Colour (178, 162, 112));
    cream.addColour (0.82, Colour (120, 100, 60));
    g.setGradientFill (cream);
    g.fillRect (win);
    ColourGradient lit (Colours::white.withAlpha (0.0f), win.getX(), 0.0f, Colours::white.withAlpha (0.0f), win.getRight(), 0.0f, false);
    lit.addColour (0.12, Colours::white.withAlpha (0.35f));
    lit.addColour (0.30, Colours::white.withAlpha (0.05f));
    lit.addColour (0.85, Colours::black.withAlpha (0.05f));
    lit.addColour (0.97, Colours::black.withAlpha (0.25f));
    g.setGradientFill (lit);
    g.fillRect (win);

    struct Mark { float db; const char* t; bool red; };
    static const Mark marks[] = { { 0, "0", true }, { -3, "3", true }, { -6, "6", true }, { -12, "12", true }, { -24, "24", true },
                                  { -36, "36", false }, { -48, "48", false } };
    const Font f = Font (FontOptions ("Times New Roman", 10.0f, Font::bold).withPointHeight (14.5f));
    for (auto m : marks)
    {
        const float y = meterY (m.db);
        g.setColour (Colour (40, 32, 22));
        g.fillRect (Rectangle<float> (609.0f, y - 0.6f, 6.0f, 1.3f) - o);
        g.fillRect (Rectangle<float> (631.0f, y - 0.6f, 6.0f, 1.3f) - o);
        g.setColour (m.red ? Colour (172, 22, 22) : Colour (58, 48, 34));
        GlyphArrangement ga;
        ga.addLineOfText (f, m.t, 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        const auto pb = p.getBounds();
        g.fillPath (p, AffineTransform::translation (at (623.0f, y).x - pb.getCentreX(), at (623.0f, y).y - pb.getCentreY()));
    }
    auto tri = [&] (float db, bool left)
    {
        const float y = db <= -99.0f ? 239.3f : jmin (239.3f, meterY (db));
        Path p;
        if (left) p.addTriangle (at (608.0f, y - 3.5f), at (617.5f, y), at (608.0f, y + 3.5f));
        else p.addTriangle (at (637.5f, y - 3.5f), at (628.0f, y), at (637.5f, y + 3.5f));
        g.setColour (Colour (196, 14, 14));
        g.fillPath (p);
        g.setColour (Colour (110, 0, 0));
        g.strokePath (p, PathStrokeType (0.6f));
    };
    tri (l, true);
    tri (two ? r : l, false);
}

//==============================================================================
void WDelayEditor::paintContent (Graphics& g)
{
    drawPanel (g, { 0, 19, W, H - 19 }, panelCache, PanelLook::hDelay);

    // scales printed on the panel (fitted on the reference: 21 ticks of 14.1 degrees; the radii of each size)
    const float s0 = refToJuce (39.0f), s1 = refToJuce (321.0f);
    auto ticks = [&] (Point<float> c, int n, float rIn, float rOut, float thick)
    {
        Scale s; s.ticks = n; s.rIn = rIn; s.rOut = rOut; s.thick = thick;
        drawScale (g, c, s0, s1, s);
    };
    ticks (kDelay, 21, 64.3f, 72.2f, 1.9f);
    ticks (kFeedback, 21, 64.3f, 72.2f, 1.9f);
    for (auto c : { kDepth, kRate, kHiPass, kLoPass }) ticks (c, 21, 33.5f, 38.7f, 1.8f);
    ticks (kMix, 21, 24.7f, 30.6f, 1.6f);
    ticks (kOutput, 21, 24.7f, 30.6f, 1.6f);
    ticks (kAnalog, 5, 23.9f, 31.3f, 1.7f);

    // printed text: each at the centre of its ink as measured on the reference
    g.setColour (kPrint);
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
            p.startNewSubPath (x0, 272.5f); p.lineTo (x0, 280.5f); p.lineTo (t0, 280.5f);
            p.startNewSubPath (t1, 280.5f); p.lineTo (x1, 280.5f); p.lineTo (x1, 272.5f);
            g.strokePath (p, PathStrokeType (1.5f));
        };
        bracket (163.5f, 298.5f, 172.0f, 291.0f);
        bracket (427.5f, 525.5f, 435.0f, 518.0f);
        g.drawLine (441.0f, 221.0f, 452.5f, 221.0f, 1.3f);
        g.drawLine (490.5f, 221.0f, 509.0f, 221.0f, 1.3f);
    }

    // the clip light above the meter (measured: a dark glass dome of radius 8.5 in a grey bezel to 12, a dull red rim
    // low on the right; lit: red)
    {
        const auto c = kClipLamp;
        ColourGradient bz (Colour (214, 212, 208), c.x - 8.0f, c.y - 9.0f, Colour (40, 38, 38), c.x + 8.0f, c.y + 9.0f, false);
        bz.addColour (0.40, Colour (120, 118, 116));
        bz.addColour (0.62, Colour (52, 50, 50));
        g.setGradientFill (bz);
        g.fillEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f, 0.8f);
        Path rim;
        rim.addCentredArc (c.x, c.y, 9.4f, 9.4f, 0.0f, rad (80.0f), rad (200.0f), true);
        g.setColour (Colour (168, 24, 24));
        g.strokePath (rim, PathStrokeType (1.4f));
        ColourGradient dome (clipLevel > 0.0 ? Colour (255, 60, 40) : Colour (70, 52, 52), c.x - 3.0f, c.y - 4.0f,
                             clipLevel > 0.0 ? Colour (120, 0, 0) : Colour (10, 6, 6), c.x + 6.0f, c.y + 7.0f, true);
        g.setGradientFill (dome);
        g.fillEllipse (c.x - 8.5f, c.y - 8.5f, 17.0f, 17.0f);
        g.setColour (Colour (230, 226, 220).withAlpha (0.85f));
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 3.4f, 2.8f);
    }
    drawLogo (g, { 21.0f, 250.5f }, "W", "DELAY", 0.74f);
}
