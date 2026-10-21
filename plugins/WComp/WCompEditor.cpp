#include "WCompEditor.h"

using namespace juce;
using namespace wavy::hybrid;

namespace
{
// design grid (the reference's GUIXML and screenshot, reference/wcomp)
const Rectangle<float> kHousing (177.0f, 53.0f, 290.0f, 222.0f);
const Point<float> kThrCentre (321.0f, 331.0f);
constexpr float kArch = 84.0f;
const Rectangle<int> kDial (203, 82, 239, 136);
const Rectangle<float> kLed (505.0f, 405.0f, 92.0f, 32.0f);
const Point<float> kLimLamp (444.5f, 75.5f);

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
        // the component covers the knob and its value slot (which can lie outside the reference's control rectangle)
        const auto b = valueArea.isEmpty() ? r : r.getUnion (valueArea);
        k.centre = c + (r.getPosition() - b.getPosition()).toFloat();
        k.valueArea = valueArea.isEmpty() ? Rectangle<int>() : valueArea - b.getPosition();
        k.format = std::move (fmt); k.selection = &selection;
        attachSlider (k, id, b, reset);
        if (k.format) wavy::flat::applyFormat (k, k.format);
    };
    analog.steps = 5;
    place (analog, wcomp::analog, { 39, 30, 97, 107 }, { 49.0f, 49.0f }, 2.0, {}, {});
    place (output, wcomp::output, { 505, 30, 97, 107 }, { 49.0f, 49.0f }, 0.0, { 599, 85, 37, 13 }, [] (double v) { return String (v, 1); });
    place (ratio, wcomp::ratio, { 6, 142, 161, 143 }, { 80.0f, 76.0f }, 3.0, { 68, 213, 37, 13 }, [] (double v) { return String (v, 2); });
    place (mix, wcomp::mix, { 473, 142, 161, 143 }, { 80.0f, 76.0f }, 100.0, { 534, 213, 37, 13 }, [] (double v) { return String (v, 1); });
    place (threshold, wcomp::threshold, { 238, 249, 169, 180 }, { 83.0f, 82.0f }, -18.0, { 302, 330, 37, 13 }, [] (double v) { return String (v, 1); });
    place (punch, wcomp::punch, { 9, 294, 97, 107 }, { 49.0f, 49.0f }, 1.0, { 39, 396, 37, 13 }, [] (double v) { return String (v, 1); });
    place (attack, wcomp::attack, { 114, 295, 96, 106 }, { 49.0f, 49.0f }, 7.0, { 143, 396, 37, 13 },
           [] (double v) { return wcomp::timeText ((float) v).upToFirstOccurrenceOf (" ", false, false); });
    place (release, wcomp::release, { 431, 294, 97, 107 }, { 49.0f, 49.0f }, 100.25, { 462, 398, 37, 13 },
           [] (double v) { return wcomp::timeText ((float) v).upToFirstOccurrenceOf (" ", false, false); });
    place (releaseNote, wcomp::releaseNote, { 431, 294, 97, 107 }, { 49.0f, 49.0f }, 5.0, { 462, 398, 37, 13 }, {});
    releaseNote.textFromValueFunction = [] (double v) { static const char* n[] = { "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T",
        "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 Bar", "2 Bar" }; return String (n[jlimit (0, 19, roundToInt (v))]); };
    releaseNote.steps = 20;

    auto& meterSel = make<ButtonGroup> (*p.apvts.getParameter (wcomp::meter), &p.undo, StringArray { "IN", "GR", "OUT" }, false);
    addToContent (meterSel, { 243, 23, 156, 33 });
    auto& lim = make<ButtonGroup> (*p.apvts.getParameter (wcomp::limiter), &p.undo, StringArray { "LIMITER" }, false, true);
    addToContent (lim, { 414, 23, 56, 34 });
    auto& sync = make<ButtonGroup> (*p.apvts.getParameter (wcomp::sync), &p.undo, StringArray { "BPM", "HOST", "MS" }, true);
    addToContent (sync, { 542, 310, 59, 71 });

    // the BPM display takes drags and typed values in BPM mode (an invisible slider over it)
    bpmField.setSliderStyle (Slider::LinearBarVertical);
    bpmField.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    bpmField.setAlpha (0.0f);
    attachSlider (bpmField, wcomp::bpm, kLed.toNearestInt(), 120.0);
    bpmField.setMouseDragSensitivity (270);
    vu.pivot = { 320.0f - (float) kDial.getX(), 222.0f - (float) kDial.getY() };
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
}

void WCompEditor::paintContent (Graphics& g)
{
    drawPanel (g, { 0, 19, W, H - 19 }, panelCache);
    drawVuHousing (g, kHousing, kThrCentre, kArch);
    {
        // the panel shows through the housing's arch around Threshold
        Path arch;
        arch.addEllipse (kThrCentre.x - kArch, kThrCentre.y - kArch, kArch * 2.0f, kArch * 2.0f);
        g.saveState();
        g.reduceClipRegion (arch);
        drawPanel (g, { 0, 19, W, H - 19 }, panelCache);
        g.restoreState();
        g.saveState();
        g.reduceClipRegion (kHousing.toNearestInt());                // the arch's lip, only inside the housing
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawEllipse (kThrCentre.x - kArch, kThrCentre.y - kArch, kArch * 2.0f, kArch * 2.0f, 2.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawEllipse (kThrCentre.x - kArch - 2.0f, kThrCentre.y - kArch - 2.0f, kArch * 2.0f + 4.0f, kArch * 2.0f + 4.0f, 1.0f);
        g.restoreState();
    }
    // the limiter lamp in the housing's corner (lit while the limiter acts; Phase 4)
    {
        const auto c = kLimLamp; const float r = 9.5f;
        g.setColour (Colour (60, 60, 62)); g.fillEllipse (c.x - r - 1.5f, c.y - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);
        ColourGradient lg (Colour (70, 66, 64), c.x - r * 0.4f, c.y - r * 0.5f, Colour (4, 4, 4), c.x + r * 0.5f, c.y + r * 0.6f, true);
        g.setGradientFill (lg); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (Colours::white.withAlpha (0.55f)); g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.45f, r * 0.35f);
    }

    // scales printed on the panel
    auto smallScale = [] (std::initializer_list<std::pair<int, const char*>> l)
    {
        Scale s; s.ticks = 21; s.rIn = 35.5f; s.rOut = 44.5f; s.thick = 1.8f; s.labelSize = 10.5f;
        for (auto [t, txt] : l)
        {
            const bool low = t <= 2 || t >= 18;                      // the end labels sit below the scale's ends
            s.labels.push_back ({ t, txt, t == 10 ? 51.0f : 50.0f, low ? (t <= 2 ? -3.0f : 3.0f) : 0.0f, low ? 4.0f : 0.0f });
        }
        return s;
    };
    const float s0 = refToJuce (39.0f), s1 = refToJuce (321.0f);
    drawScale (g, { 58.0f, 343.0f }, s0, s1, smallScale ({ { 0, "0" }, { 10, "15" }, { 20, "30" } }));
    drawScale (g, { 163.0f, 344.0f }, s0, s1, smallScale ({ { 0, "0.5" }, { 10, "7" }, { 20, "100" } }));
    if (lastSync == 2) drawScale (g, { 480.0f, 343.0f }, s0, s1, smallScale ({ { 0, "3" }, { 10, "100" }, { 20, "3S" } }));
    else drawScale (g, { 480.0f, 343.0f }, s0, s1, smallScale ({ { 0, "1/64" }, { 20, "2 Bar" } }));
    drawScale (g, { 554.0f, 79.0f }, s0, s1, smallScale ({ { 0, "-18" }, { 5, "-9" }, { 10, "0" }, { 15, "+9" }, { 20, "+18" } }));
    {
        Scale a; a.ticks = 5; a.rIn = 37.0f; a.rOut = 45.0f;
        a.labelSize = 10.5f;
        a.labels = { { 0, "OFF", 49.0f, -2.0f, -2.0f }, { 1, "1", 45.0f }, { 2, "2", 46.0f }, { 3, "3", 43.0f }, { 4, "4", 44.0f } };
        drawScale (g, { 88.0f, 79.0f }, s0, s1, a);
    }
    auto bigScale = [] (std::initializer_list<std::pair<int, const char*>> l)
    {
        Scale s; s.ticks = 21; s.rIn = 58.0f; s.rOut = 68.0f; s.thick = 2.0f; s.labelSize = 12.0f;
        for (auto [t, txt] : l) s.labels.push_back ({ t, txt, 76.0f });
        return s;
    };
    const float b0 = refToJuce (40.0f), b1 = refToJuce (320.0f);
    drawScale (g, { 86.0f, 218.0f }, b0, b1, bigScale ({ { 0, "1" }, { 10, "8" }, { 20, "50" } }));
    drawScale (g, { 553.0f, 218.0f }, b0, b1, bigScale ({ { 0, "0" }, { 2, "10" }, { 4, "20" }, { 6, "30" }, { 8, "40" }, { 10, "50" },
                                                        { 12, "60" }, { 14, "70" }, { 16, "80" }, { 18, "90" }, { 20, "100" } }));
    {
        Scale t; t.ticks = 17; t.rIn = 74.0f; t.rOut = 82.0f; t.thick = 2.0f;
        drawScale (g, kThrCentre, refToJuce (53.0f), refToJuce (307.0f), t);
    }

    // labels
    g.setColour (kPrint);
    auto label = [&] (const char* t, float cx, float cy, float size = 14.0f)
    {
        g.setFont (font (size).withHorizontalScale (1.16f));
        g.drawText (t, Rectangle<float> (cx - 60.0f, cy - 9.0f, 120.0f, 18.0f), Justification::centred);
    };
    label ("ANALOG", 88.0f, 127.0f, 12.5f); label ("OUTPUT", 554.0f, 127.0f, 12.5f);
    label ("RATIO", 88.0f, 282.0f, 12.5f); label ("MIX", 554.0f, 282.0f, 12.5f);
    label ("DRY", 498.0f, 282.0f, 11.0f); label ("WET", 604.0f, 282.0f, 11.0f);
    label ("PUNCH", 58.0f, 389.0f, 12.5f); label ("ATTACK", 162.0f, 389.0f, 12.5f);
    label ("RELEASE", 480.0f, 389.0f, 12.5f); label ("SYNC", 571.0f, 389.0f, 12.5f);
    label ("[dB]", 321.0f, 435.0f, 13.0f);
    // "0  THRESHOLD  -48" along the arc under the knob
    {
        const String t = "0   THRESHOLD   -48";
        const Font f = font (16.5f).withHorizontalScale (1.08f);
        g.setFont (f);
        const float r = 91.0f, total = GlyphArrangement::getStringWidth (f, t);
        float s = -total * 0.5f;
        for (int i = 0; i < t.length(); ++i)
        {
            const String ch = t.substring (i, i + 1);
            const float w = GlyphArrangement::getStringWidth (f, ch);
            const float a = (s + w * 0.5f) / r;                          // 0 = straight down
            const Point<float> pt (kThrCentre.x - r * std::sin (-a), kThrCentre.y + r * std::cos (-a));
            g.saveState();
            g.addTransform (AffineTransform::rotation (-a, pt.x, pt.y));
            g.drawText (ch, Rectangle<float> (pt.x - w, pt.y - 8.0f, w * 2.0f, 16.0f), Justification::centred);
            g.restoreState();
            s += w;
        }
    }
    // the connector from RELEASE to the BPM display, the display, the logo
    g.setColour (Colour (24, 14, 6));
    g.drawLine (480.0f, 397.0f, 480.0f, 421.0f, 1.6f);
    g.drawLine (480.0f, 421.0f, 505.0f, 421.0f, 1.6f);
    const int sync = roundToInt (value (wcomp::sync));
    if (sync == 2) drawLedDisplay (g, kLed, "BPM OFF");
    else drawLedDisplay (g, kLed, "BPM", sync == 0 ? String (roundToInt (value (wcomp::bpm)))
                                                    : proc.hostBpm.load() > 0.0 ? String (roundToInt (proc.hostBpm.load())) : String ("---"));
    drawLogo (g, { 50.0f, 404.0f, 130.0f, 30.0f }, "W", "COMP");
}
