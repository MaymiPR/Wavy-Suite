#include "WVoxEditor.h"

using namespace juce;
using namespace wavy::ren;

WVoxEditor::WVoxEditor (WVoxProcessor& p)
    : Editor (p, p.apvts, p.undo, p.apvts.getParameter (wvox::mode), "WVox", JucePlugin_VersionString, W, H), proc (p),
      comp (makeFader (Fader::Big)), gate (makeFader (Fader::Small, 4)), gain (makeFader (Fader::Small, 5))
{
    gate.format = [] (double v) { return dbText ((float) v, true); };
    attach (comp, wvox::comp, { 135, 99, 102, 207 }, 0.0);
    attach (gate, wvox::gate, { 37, 89, 42, 211 }, -80.0);
    attach (gain, wvox::gain, { 292, 89, 42, 211 }, 0.0);
}

Rectangle<int> WVoxEditor::vuRect (bool right) const { return { isMono() ? 311 : (right ? 315 : 307), 122, 5, 155 }; }
Rectangle<int> WVoxEditor::vuTextRect (bool right) const { return { isMono() ? 296 : (right ? 315 : 278), 303, 33, 14 }; }

void WVoxEditor::tick()
{
    energy = proc.energyDb.load(); attn = proc.attnDb.load();
    outL = proc.outLDb.load();     outR = proc.outRDb.load();
    energyHold = jmax (energyHold, energy); attnHold = jmin (attnHold, attn);
    outLHold = jmax (outLHold, outL);       outRHold = jmax (outRHold, outR);
    content().repaint (30, 100, 310, 240);
}

void WVoxEditor::contentMouseDown (const MouseEvent& e)
{
    // as in the reference: a click on any meter or readout restarts all the peak holds (they then read -Inf)
    const auto p = e.getPosition();
    const Rectangle<int> areas[] = { { 39, 303, 33, 14 }, { 148, 287, 74, 23 }, vuTextRect (false), vuTextRect (true),
                                     { 50, 121, 15, 156 }, { 145, 121, 82, 155 }, vuRect (false), vuRect (true) };
    for (auto& a : areas)
        if (a.expanded (2).contains (p))
        {
            energyHold = outLHold = outRHold = -80.0f;
            attnHold = 0.0f;
            content().repaint();
            break;
        }
}

void WVoxEditor::paintContent (Graphics& g)
{
    paintPanel (g);

    // logo: script W + VOX
    g.setColour (Colour (170, 176, 182));
    g.setFont (Font (FontOptions ("Georgia", 26.0f, Font::italic)));
    g.drawText ("W", Rectangle<int> (146, 46, 34, 30), Justification::centred);
    g.setColour (Colour (159, 26, 42));
    g.setFont (arial (22.0f));
    g.drawText ("VOX", Rectangle<int> (178, 49, 48, 26), Justification::centredLeft);

    paintHousing (g);
    // total compression window: reduction shows red, from the top
    paintWindow (g, -attn / 36.0f, kGrRed, kGrRedLine, 5, 146.0f, 26.0f);

    // labels
    g.setFont (arial (15.0f));
    g.setColour (kLabel);
    g.drawText ("Gate", Rectangle<int> (28, 74, 60, 18), Justification::centred);
    g.drawText ("Gain", Rectangle<int> (282, 74, 60, 18), Justification::centred);
    g.drawText ("Comp", Rectangle<int> (155, 88, 60, 18), Justification::centred);
    g.drawText ("Energy", Rectangle<int> (28, 283, 60, 18), Justification::centred);
    g.drawText ("Output", Rectangle<int> (282, 283, 60, 18), Justification::centred);

    // scales
    g.setFont (arial (12.5f));
    g.setColour (kScale);
    for (int i = 0; i < 6; ++i)
    {
        const float y = 145.5f + 23.0f * (float) i;
        g.setColour (kScale);
        g.drawText (String (12 * (i + 1)), Rectangle<float> (20.0f, y - 7.0f, 20.0f, 14.0f), Justification::centredRight);
        g.fillRect (43.0f, y, 4.0f, 1.0f);
        g.setColour (kScaleInner);
        const float cy = 146.0f + 25.8f * (float) i;
        g.drawText (String (6 * (i + 1)), Rectangle<float> (104.0f, cy - 7.0f, 20.0f, 14.0f), Justification::centredRight);
        g.fillRect (126.0f, cy, 4.0f, 1.0f);
    }

    // meters
    paintCapsule (g, { 50.0f, 121.0f, 15.0f, 156.0f }, (energy + 80.0f) / 80.0f);
    paintCapsule (g, vuRect (false).toFloat(), (outL + 36.0f) / 36.0f);
    if (! isMono()) paintCapsule (g, vuRect (true).toFloat(), (outR + 36.0f) / 36.0f);

    // peak-hold readouts
    g.setColour (kReadout);
    g.setFont (arial (12.5f));
    g.drawText (dbText (energyHold, true), Rectangle<int> (39, 303, 33, 14), Justification::centred);
    g.drawText (dbText (outLHold, true), vuTextRect (false), Justification::centred);
    if (! isMono()) g.drawText (dbText (outRHold, true), vuTextRect (true), Justification::centred);
    g.setFont (arial (16.0f));
    g.drawText (attnHold > -0.05f ? String ("-Inf") : dbText (attnHold), Rectangle<int> (148, 287, 74, 23), Justification::centred);
}
