#pragma once
#include "WVoxProcessor.h"
#include "WavyToolbar.h"

// WVox editor. Designed on a 371 x 357 px grid (dark skin); the window can be resized
// at that aspect ratio and everything scales with it.
class WVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WVoxEditor (WVoxProcessor&);
    ~WVoxEditor() override;

    void resized() override;

    static constexpr int W = 371, H = 357;

private:
    class Fader;
    class Look;
    class Content;
    void paintContent (juce::Graphics&);
    void contentMouseDown (const juce::MouseEvent&);
    void timerCallback() override;
    bool isMono() const;
    juce::Rectangle<int> vuRect (bool right) const;
    juce::Rectangle<int> vuTextRect (bool right) const;

    WVoxProcessor& proc;
    std::unique_ptr<Look> look;
    std::unique_ptr<Content> content;
    std::unique_ptr<Fader> comp, gate, gain;
    std::unique_ptr<juce::SliderParameterAttachment> compAtt, gateAtt, gainAtt;
    std::unique_ptr<wavy::Toolbar> toolbar;
    juce::ComponentBoundsConstrainer constrainer;

    float energy = -80.0f, attn = 0.0f, outL = -80.0f, outR = -80.0f;
    float energyHold = -80.0f, attnHold = 0.0f, outLHold = -80.0f, outRHold = -80.0f;
};
