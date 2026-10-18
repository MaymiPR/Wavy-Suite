#pragma once
#include "WBassProcessor.h"
#include "WavyRenUI.h"

// WBass editor: 371 x 392 px design grid (the reference's dark skin), resizable at that aspect ratio.
class WBassEditor : public wavy::ren::Editor
{
public:
    explicit WBassEditor (WBassProcessor&);

    static constexpr int W = 371, H = 392;

private:
    void paintContent (juce::Graphics&) override;
    void contentMouseDown (const juce::MouseEvent&) override;
    void tick() override;
    juce::Rectangle<int> vuRect (bool right) const;
    juce::Rectangle<int> vuTextRect (bool right) const;

    WBassProcessor& proc;
    wavy::ren::Fader& intensity;
    wavy::ren::Fader& gain;
    wavy::ren::Fader& freq;
    wavy::ren::InSwitch& origBass;

    wavy::MeterBallistics origBar, harmBar, outLBar, outRBar;
    double lastTick = 0.0;
    float orig = -80.0f, harm = -80.0f, outL = -80.0f, outR = -80.0f;
    float origHold = -80.0f, harmHold = -80.0f, outLHold = -80.0f, outRHold = -80.0f;
    bool clip = false;
};
