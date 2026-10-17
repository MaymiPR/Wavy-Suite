#pragma once
#include "WVoxProcessor.h"
#include "WavyToolbar.h"

// Wavy WVox editor: 371 x 357 px, dark skin, laid out on the reference's pixel grid.
class WVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WVoxEditor (WVoxProcessor&);
    ~WVoxEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int W = 371, H = 357;

private:
    class Fader;
    class Look;
    void timerCallback() override;
    bool isMono() const;
    juce::Rectangle<int> vuRect (bool right) const;
    juce::Rectangle<int> vuTextRect (bool right) const;

    WVoxProcessor& proc;
    std::unique_ptr<Look> look;
    std::unique_ptr<Fader> comp, gate, gain;
    std::unique_ptr<juce::SliderParameterAttachment> compAtt, gateAtt, gainAtt;
    wavy::Toolbar toolbar;

    float energy = -80.0f, attn = 0.0f, outL = -80.0f, outR = -80.0f;
    float energyHold = -80.0f, attnHold = 0.0f, outLHold = -80.0f, outRHold = -80.0f;
};
