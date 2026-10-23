#include "WCompEditor.h"

using namespace juce;
using namespace wavy::hybrid;

namespace
{
// design grid (the reference's GUIXML and screenshot, reference/wcomp)
const Rectangle<float> kHousing (174.5f, 52.5f, 295.0f, 228.0f);   // its outline, bevels included
constexpr float kArch = 83.2f;                                  // the housing's arch around Threshold (measured: half a pixel
const Point<float> kArchCentre (320.15f, 333.35f);              // left of the knob's centre)
const Rectangle<int> kDial (197, 73, 251, 170);                 // the dial's window frame, and the pivot's bump below it
const Point<float> kVuPivot (320.7f, 220.9f);                  // the scale's centre
const Rectangle<float> kLed (506.0f, 404.0f, 90.5f, 34.0f);
const Point<float> kLimLamp (444.5f, 75.5f);
// knob centres (the knob bodies in the reference: rings fitted on the captures)
const Point<float> kAnalog (88.23f, 80.10f), kOutput (554.17f, 79.99f), kRatio (86.07f, 218.47f), kMix (552.58f, 218.48f),
    kThreshold (320.65f, 333.35f), kPunch (58.25f, 344.08f), kAttack (163.17f, 344.99f), kRelease (480.18f, 343.95f);

float rad (float deg) { return deg * MathConstants<float>::pi / 180.0f; }
float refToJuce (float deg) { return rad (deg + 180.0f); }
}

WCompEditor::WCompEditor (WCompProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wcomp::mode), "WComp", JucePlugin_VersionString, W, H,
              wavy::Toolbar::Style::Hybrid),
      proc (p),
      analog (make<Knob> (Knob::Small, 39.0f, 321.0f)), output (make<Knob> (Knob::Small, 39.0f, 321.0f)),
      ratio (make<Knob> (Knob::Large, 40.0f, 320.0f)), mix (make<Knob> (Knob::Large, 40.0f, 320.0f)),
      threshold (make<Knob> (Knob::Huge, 53.0f, 307.0f)),       // 0 dB on the left, -48 dB on the right (inverted parameter)
      punch (make<Knob> (Knob::Small, 39.0f, 321.0f)), attack (make<Knob> (Knob::Small, 39.0f, 321.0f)),
      release (make<Knob> (Knob::Small, 39.0f, 321.0f)), releaseNote (make<Knob> (Knob::Small, 39.0f, 321.0f)),
      vu (make<VuMeter>())
{
    setAuxLook (std::make_unique<HybridLook>());
    for (auto* c : getChildren())
        if (dynamic_cast<ResizableCornerComponent*> (c) != nullptr) c->setAlpha (0.0f);
    setFactoryPresets (wcomp::factoryPresets());
    setKeepOnPresetLoad ({ p.apvts.getParameter (wcomp::mix) });        // presets leave Mix alone, as the reference

    auto place = [this] (Knob& k, const char* id, Rectangle<int> r, Point<float> c, double reset, Rectangle<int> valueArea,
                         std::function<String (double)> fmt)
    {
        // the component covers the knob, its shadow and its value slot (which can lie outside the reference's control
        // rectangle); clicks outside the knob fall through (hitTest)
        const float e = k.paintExtent();
        const auto drawn = Rectangle<float> (c.x - e, c.y - e, 2.0f * e, 2.0f * e).getSmallestIntegerContainer();
        const auto b = (valueArea.isEmpty() ? r : r.getUnion (valueArea)).getUnion (drawn);
        k.centre = c - b.getPosition().toFloat();
        k.valueArea = valueArea.isEmpty() ? Rectangle<int>() : valueArea - b.getPosition();
        k.format = std::move (fmt); k.selection = &selection;
        attachSlider (k, id, b, reset);
        // a knob whose range is never positive (Threshold) takes "18" as -18: the reference's ParamXML flags it
        // PositiveTextEntryToNegative (only Threshold)
        if (k.format) wavy::flat::applyFormat (k, k.format, k.getMaximum() <= 0.0);
    };
    analog.steps = 5;
    place (analog, wcomp::analog, { 39, 30, 97, 107 }, kAnalog, 2.0, {}, {});
    place (output, wcomp::output, { 505, 30, 97, 107 }, kOutput, 0.0, { 599, 85, 37, 13 }, [] (double v) { return String (v, 1); });
    place (ratio, wcomp::ratio, { 6, 142, 161, 143 }, kRatio, 3.0, { 68, 213, 37, 13 }, [] (double v) { return String (v, 2); });
    place (mix, wcomp::mix, { 473, 142, 161, 143 }, kMix, 100.0, { 534, 213, 37, 13 }, [] (double v) { return String (v, 1); });
    place (threshold, wcomp::threshold, { 238, 249, 169, 180 }, kThreshold, -18.0, { 302, 330, 37, 13 }, [] (double v) { return String (v, 1); });
    place (punch, wcomp::punch, { 9, 294, 97, 107 }, kPunch, 1.0, { 39, 396, 37, 13 }, [] (double v) { return String (v, 1); });
    place (attack, wcomp::attack, { 114, 295, 96, 106 }, kAttack, 7.0, { 143, 396, 37, 13 },
           [] (double v) { return wcomp::timeText ((float) v).upToFirstOccurrenceOf (" ", false, false); });
    place (release, wcomp::release, { 431, 294, 97, 107 }, kRelease, 100.25, { 462, 398, 37, 13 },
           [] (double v) { return wcomp::timeText ((float) v).upToFirstOccurrenceOf (" ", false, false); });
    place (releaseNote, wcomp::releaseNote, { 431, 294, 97, 107 }, kRelease, 5.0, { 462, 398, 37, 13 }, {});
    releaseNote.textFromValueFunction = [] (double v) { static const char* n[] = { "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T",
        "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 Bar", "2 Bar" }; return String (n[jlimit (0, 19, roundToInt (v))]); };
    releaseNote.steps = 20;

    auto& meterSel = make<ButtonGroup> (*p.apvts.getParameter (wcomp::meter), &p.undo, StringArray { "IN", "GR", "OUT" }, false);
    addToContent (meterSel, Rectangle<int> (245, 27, 151, 21).expanded (4));
    auto& lim = make<ButtonGroup> (*p.apvts.getParameter (wcomp::limiter), &p.undo, StringArray { "LIMITER" }, false, true);
    addToContent (lim, Rectangle<int> (417, 27, 52, 21).expanded (4));
    auto& sync = make<ButtonGroup> (*p.apvts.getParameter (wcomp::sync), &p.undo, StringArray { "BPM", "HOST", "MS" }, true);
    addToContent (sync, Rectangle<int> (547, 315, 50, 61).expanded (4));

    // the BPM display takes drags and typed values in BPM mode (an invisible slider over it)
    bpmField.setSliderStyle (Slider::LinearBarVertical);
    bpmField.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    bpmField.setAlpha (0.0f);
    attachSlider (bpmField, wcomp::bpm, kLed.toNearestInt(), 120.0);
    bpmField.setMouseDragSensitivity (270);
    vu.pivot = kVuPivot - kDial.getPosition().toFloat();
    addToContent (vu, kDial);
    updateReleaseKnob();
}

float WCompEditor::value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void WCompEditor::updateReleaseKnob()
{
    const int s = roundToInt (value (wcomp::sync));
    if (s == lastSync) return;
    lastSync = s;
    release.setVisible (s == 2);                     // ms
    releaseNote.setVisible (s != 2);                 // BPM / Host: note values
    bpmField.setEnabled (s == 0);                    // the tempo is set here only in BPM mode
    content().repaint();
}

void WCompEditor::tick()
{
    updateReleaseKnob();
    content().repaint (kLed.toNearestInt().expanded (2));

    const double now = Time::getMillisecondCounterHiRes() * 1.0e-3;
    const double dt = lastTick > 0.0 ? jlimit (0.0, 0.1, now - lastTick) : 1.0 / 60.0;
    lastTick = now;

    // the needle: GR reads the compressor's reduction on the printed scale; In / Out read the level below full scale on
    // the same scale (0 dBFS at the right end). Provisional ballistics (not measured yet): a VU-like damped movement,
    // 99 % in about 300 ms
    const float gr = jmax (0.0f, proc.grDb.take());
    const float in = jmax (proc.inLDb.take(), proc.inRDb.take()), out = jmax (proc.outLDb.take(), proc.outRDb.take());
    const int m = roundToInt (value (wcomp::meter));
    const float shown = m == 1 ? gr : jlimit (0.0f, 48.0f, -(m == 0 ? in : out));
    const double target = VuMeter::angleForGr (shown);
    constexpr double w = 22.0, zeta = 0.8;
    for (int k = 0; k < 4; ++k)                           // a few sub-steps for stability at the 60 Hz frame rate
    {
        const double h = dt / 4.0;
        needleVel += (w * w * (target - needleAng) - 2.0 * zeta * w * needleVel) * h;
        needleAng += needleVel * h;
    }
    vu.setNeedle ((float) needleAng);

    // the limiter lamp: lit while the limiter reduces, held briefly so short catches show
    const float lim = proc.limDb.take();
    limHold = lim > 0.05f ? 0.15 : jmax (0.0, limHold - dt);
    const bool lit = limHold > 0.0;
    if (lit != lampLit) { lampLit = lit; content().repaint (Rectangle<float> (kLimLamp.x - 12.0f, kLimLamp.y - 12.0f, 24.0f, 24.0f).toNearestInt()); }
}

void WCompEditor::paintContent (Graphics& g)
{
    drawPanel (g, { 0, 19, W, H - 19 }, panelCache);
    drawVuHousing (g, kHousing, kArchCentre, kArch);
    // the limiter lamp in the housing's corner (lit while the limiter acts), measured: a black glass dome of
    // radius 8.6 at (444.5, 75.5) in a bezel to 11.2, a bright crescent on its upper left fading to a dull reddish rim on
    // its lower right; two small reflections on the dome
    {
        const auto c = kLimLamp;
        ColourGradient bz (Colour (240, 234, 216), c.x - 7.0f, c.y - 8.0f, Colour (92, 54, 46), c.x + 7.0f, c.y + 8.0f, false);
        bz.addColour (0.38, Colour (150, 146, 138));
        bz.addColour (0.55, Colour (42, 40, 40));
        g.setGradientFill (bz);
        g.fillEllipse (c.x - 11.2f, c.y - 11.2f, 22.4f, 22.4f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - 11.2f, c.y - 11.2f, 22.4f, 22.4f, 0.7f);
        g.setColour (Colour (2, 2, 2));
        g.fillEllipse (c.x - 8.6f, c.y - 8.6f, 17.2f, 17.2f);
        if (lampLit)                                      // lit: a red glow in the dome (the lit look is not measured)
        {
            ColourGradient glow (Colour (255, 92, 60), c.x - 1.5f, c.y - 1.5f, Colour (120, 10, 4), c.x + 8.6f, c.y + 8.6f, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - 8.6f, c.y - 8.6f, 17.2f, 17.2f);
        }
        g.setColour (Colour (232, 228, 220));
        g.fillEllipse (c.x - 5.0f, c.y - 4.4f, 3.6f, 3.0f);
        g.setColour (Colour (150, 148, 144));
        g.fillEllipse (c.x - 0.4f, c.y - 5.4f, 2.0f, 1.8f);
    }

    // scales printed on the panel (fitted on the reference: tick ring, radii, thickness; 14 degrees per tick)
    const float s0 = refToJuce (40.0f), s1 = refToJuce (320.0f);
    auto ticks = [&] (Point<float> c, int n, float a0, float a1, float rIn, float rOut, float thick)
    {
        Scale s; s.ticks = n; s.rIn = rIn; s.rOut = rOut; s.thick = thick;
        drawScale (g, c, a0, a1, s);
    };
    const Point<float> smallTicks (-0.3f, -0.4f);              // the small knobs' tick rings sit a little up-left of the knobs
    ticks (kAnalog + smallTicks, 5, s0, s1, 33.8f, 40.0f, 1.7f);
    ticks (kOutput + smallTicks, 21, s0, s1, 33.8f, 40.0f, 1.7f);
    ticks (kPunch + smallTicks, 21, s0, s1, 33.8f, 40.0f, 1.7f);
    ticks (kAttack + smallTicks, 21, s0, s1, 33.8f, 40.0f, 1.7f);
    ticks (kRelease + smallTicks, 21, s0, s1, 33.8f, 40.0f, 1.7f);
    ticks (kRatio + Point<float> (0.0f, 0.4f), 21, s0, s1, 56.2f, 64.3f, 1.65f);
    ticks (kMix + Point<float> (0.0f, 0.4f), 21, s0, s1, 56.2f, 64.3f, 1.65f);
    ticks ({ 320.7f, 332.9f }, 19, refToJuce (54.0f), refToJuce (306.0f), 72.2f, 80.4f, 1.7f);

    // printed text: each at the centre of its ink as measured on the reference
    g.setColour (kPrint);
    auto label = [&] (const char* t, float x, float y) { printText (g, t, { x, y }, 10.75f, 1.2f, true); };
    auto num = [&] (const char* t, float x, float y, float em = 8.6f) { printText (g, t, { x, y }, em, 1.15f, false); };
    label ("ANALOG", 88.5f, 125.9f); label ("OUTPUT", 554.6f, 126.0f);
    label ("RATIO", 88.25f, 282.1f); label ("MIX", 552.0f, 282.0f);
    label ("PUNCH", 58.1f, 389.1f); label ("ATTACK", 162.0f, 389.25f);
    label ("RELEASE", 480.75f, 389.0f); label ("SYNC", 571.0f, 389.4f);
    printText (g, "DRY", { 499.0f, 282.5f }, 9.75f, 1.07f, false);
    printText (g, "WET", { 606.75f, 282.5f }, 9.75f, 1.07f, false);
    printText (g, "[dB]", { 320.5f, 435.5f }, 12.0f, 1.2f, true);

    num ("OFF", 53.2f, 112.5f, 7.0f); num ("1", 45.25f, 65.0f); num ("2", 87.5f, 35.5f); num ("3", 128.6f, 64.9f); num ("4", 116.4f, 110.8f, 7.3f);
    num ("-18", 519.0f, 112.9f); num ("-9", 509.0f, 64.4f); num ("0", 553.75f, 34.9f); num ("+9", 598.6f, 64.25f); num ("+18", 587.1f, 112.75f);
    num ("1", 40.75f, 273.1f); num ("8", 86.25f, 148.0f); num ("50", 134.4f, 272.9f);
    {
        static const char* mixText[] = { "0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100" };
        static const float mixPos[][2] = { { 507.75f, 272.9f }, { 486.6f, 245.25f }, { 480.4f, 210.5f }, { 490.75f, 177.4f }, { 517.9f, 154.9f },
                                           { 552.9f, 146.9f }, { 587.1f, 154.9f }, { 614.1f, 177.4f }, { 625.75f, 212.0f }, { 620.5f, 244.9f },
                                           { 602.6f, 272.9f } };
        for (int i = 0; i < 11; ++i) num (mixText[i], mixPos[i][0], mixPos[i][1]);
    }
    num ("0", 28.6f, 375.2f); num ("15", 57.9f, 297.9f); num ("30", 90.0f, 375.5f);
    num ("0.5", 126.75f, 375.4f); num ("7", 162.5f, 297.9f); num ("100", 197.4f, 375.25f);
    if (lastSync == 2) { num ("3", 449.25f, 375.4f); num ("100", 478.5f, 297.9f); num ("3S", 511.5f, 375.5f); }
    else { num ("1/64", 442.5f, 375.2f); num ("2 Bar", 518.0f, 375.2f); }
    // "0  THRESHOLD  -48" around the knob: each glyph at its measured angle and radius, its top toward the centre
    {
        auto onArc = [&] (const String& t, float deg, float r)
        {
            const float a = rad (deg);
            printText (g, t, { kThreshold.x + r * std::sin (a), kThreshold.y - r * std::cos (a) }, 12.6f, 1.05f, true, a - MathConstants<float>::pi);
        };
        onArc ("0", 234.35f, 83.2f);
        static const float ang[] = { 214.85f, 205.8f, 197.05f, 189.25f, 181.25f, 172.7f, 163.6f, 155.65f, 147.35f };
        for (int i = 0; i < 9; ++i) onArc (String::charToString (String ("THRESHOLD")[i]), ang[i], 85.0f);
        onArc ("-48", 129.0f, 84.0f);
    }
    // the connector from RELEASE to the BPM display, the display, the logo
    {
        Path link;                                                  // measured: down from RELEASE, then right to the display
        link.startNewSubPath (478.75f, 396.0f);
        link.lineTo (478.75f, 406.25f);
        link.quadraticTo (478.75f, 409.25f, 481.75f, 409.25f);
        link.lineTo (502.5f, 409.25f);
        g.setColour (kPrint);
        g.strokePath (link, PathStrokeType (1.5f));
    }
    const int sync = roundToInt (value (wcomp::sync));
    if (sync == 2) drawLedDisplay (g, kLed, "BPM OFF");
    else drawLedDisplay (g, kLed, "BPM", sync == 0 ? String (roundToInt (value (wcomp::bpm)))
                                                    : proc.hostBpm.load() > 0.0 ? String (roundToInt (proc.hostBpm.load())) : String ("---"));
    drawLogo (g, { 49.6f, 421.4f }, "W", "COMP");
}
