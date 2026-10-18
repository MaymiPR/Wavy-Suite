#include "WBassEditor.h"

using namespace juce;
using namespace wavy::ren;

namespace
{
const Colour kHarm (222, 92, 64), kHarmLine (232, 130, 108);
const Rectangle<int> kClip (302, 103, 23, 13), kHarmText (143, 287, 82, 25), kOrigText (39, 303, 33, 14);
}

WBassEditor::WBassEditor (WBassProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wbass::mode), "WBass", JucePlugin_VersionString, W, H), proc (p),
      intensity (makeFader (Fader::Big)), gain (makeFader (Fader::Small, 5)), freq (makeFader (Fader::Horizontal)),
      origBass (makeSwitch())
{
    setFactoryPresets (wbass::factoryPresets());
    intensity.negateTyped = false;                                   // -24 .. +24 dB
    freq.format = [] (double v) { return String (roundToInt (v)); };
    freq.negateTyped = false;
    freq.textArea = { 129, 6, 42, 16 };
    gain.trackTop = 20;
    attach (intensity, wbass::intensity, { 135, 99, 102, 207 }, 0.0);
    attach (gain, wbass::gain, { 292, 85, 42, 215 }, 0.0);
    attach (freq, wbass::freq, { 120, 334, 171, 41 }, 80.0);
    attach (origBass, wbass::origBass, { 41, 88, 33, 38 });
}

Rectangle<int> WBassEditor::vuRect (bool right) const { return { isMono() ? 311 : (right ? 315 : 307), 122, 5, 155 }; }
Rectangle<int> WBassEditor::vuTextRect (bool right) const { return { isMono() ? 295 : (right ? 315 : 278), 303, 33, 14 }; }

void WBassEditor::tick()
{
    // the reference's ballistics (WavyMeter.h); the readouts hold the highest peak itself
    const double now = Time::getMillisecondCounterHiRes() * 0.001, dt = lastTick > 0.0 ? now - lastTick : 0.0;
    lastTick = now;
    const float o = proc.origDb.take(), h = proc.harmDb.take(), l = proc.outLDb.take(), r = proc.outRDb.take();
    orig = origBar.update (o, dt); harm = harmBar.update (h, dt);
    outL = outLBar.update (l, dt); outR = outRBar.update (r, dt);
    clip = clip || proc.clipped.exchange (false);
    origHold = jmax (origHold, o); harmHold = jmax (harmHold, h);
    outLHold = jmax (outLHold, l); outRHold = jmax (outRHold, r);
    content().repaint (30, 95, 310, 245);
}

void WBassEditor::contentMouseDown (const MouseEvent& e)
{
    const auto p = e.getPosition();
    if (kClip.expanded (2).contains (p)) { clip = false; content().repaint(); return; }     // click the clip light to reset it
    // a click on any meter or readout restarts all the peak holds (they then read -Inf)
    const Rectangle<int> areas[] = { kOrigText, kHarmText, vuTextRect (false), vuTextRect (true),
                                     { 50, 121, 15, 156 }, { 145, 121, 82, 155 }, vuRect (false), vuRect (true) };
    for (auto& a : areas)
        if (a.expanded (2).contains (p))
        {
            origHold = harmHold = outLHold = outRHold = -80.0f;
            content().repaint();
            break;
        }
}

void WBassEditor::paintContent (Graphics& g)
{
    paintPanel (g);
    paintTitle (g, "BASS", Colour (226, 85, 56));
    paintHousing (g);

    // created-harmonics window: orange-red from the bottom, 0 dB at the top, 30 dB over the window
    paintWindow (g, (harm + 30.0f) / 30.0f, kHarm, kHarmLine, 5, 136.5f, 31.0f, true);

    // labels
    g.setFont (arial (15.0f));
    g.setColour (kLabel);
    g.drawText ("Intensity", Rectangle<int> (145, 87, 82, 18), Justification::centred);
    g.drawText ("Gain", Rectangle<int> (282, 69, 60, 18), Justification::centred);
    g.drawText ("Original Bass", Rectangle<int> (8, 283, 100, 18), Justification::centred);
    g.drawText ("Output", Rectangle<int> (282, 283, 60, 18), Justification::centred);
    g.drawText ("Freq", Rectangle<int> (88, 342, 40, 18), Justification::centred);

    // centre scale (dB below 0)
    g.setFont (arial (12.5f));
    g.setColour (kScaleInner);
    for (int i = 0; i < 5; ++i)
    {
        const float y = 136.5f + 31.0f * (float) i;
        g.drawText (String (3 + 6 * i), Rectangle<float> (104.0f, y - 7.0f, 20.0f, 14.0f), Justification::centredRight);
        g.fillRect (126.0f, y, 4.0f, 1.0f);
    }

    // Freq track
    g.setColour (kTrack);
    g.fillRect (137.0f, 351.0f, 98.0f, 1.0f);

    // clip light
    paintClipLight (g, kClip.toFloat(), clip);

    // meters
    paintCapsule (g, { 50.0f, 121.0f, 15.0f, 156.0f }, (orig + 30.0f) / 30.0f);
    paintCapsule (g, vuRect (false).toFloat(), (outL + 30.0f) / 30.0f);
    if (! isMono()) paintCapsule (g, vuRect (true).toFloat(), (outR + 30.0f) / 30.0f);

    // peak-hold readouts
    g.setColour (kReadout);
    g.setFont (arial (12.5f));
    g.drawText (dbText (origHold, true), kOrigText, Justification::centred);
    g.drawText (dbText (outLHold, true), vuTextRect (false), Justification::centred);
    if (! isMono()) g.drawText (dbText (outRHold, true), vuTextRect (true), Justification::centred);
    g.setFont (arial (16.0f));
    g.drawText (dbText (harmHold, true), kHarmText, Justification::centred);
}
