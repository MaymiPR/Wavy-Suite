#pragma once
#include "WCompProcessor.h"
#include "WCompNeedle.h"
#include "WavyRenUI.h"
#include "WavyHybridUI.h"

// WComp editor: 643 x 454 px design grid (the reference's Hybrid-line editor), resizable at that aspect ratio.
class WCompEditor : public wavy::ren::Editor
{
public:
    explicit WCompEditor (WCompProcessor&);

    static constexpr int W = 643, H = 454;

private:
    void paintContent (juce::Graphics&) override;
    void tick() override;
    float value (const char* id) const;
    void updateReleaseKnob();
    void showMeter();

    WCompProcessor& proc;
    wavy::hybrid::Knob& analog;
    wavy::hybrid::Knob& output;
    wavy::hybrid::Knob& ratio;
    wavy::hybrid::Knob& mix;
    wavy::hybrid::Knob& threshold;
    wavy::hybrid::Knob& punch;
    wavy::hybrid::Knob& attack;
    wavy::hybrid::Knob& release;
    wavy::hybrid::Knob& releaseNote;
    wavy::hybrid::VuMeter& vu;
    juce::Slider& bpmField = make<juce::Slider>();
    void contentMouseDown (const juce::MouseEvent&) override { selection.clear(); }
    wavy::flat::Selection selection;
    juce::Image panelCache;
    int lastSync = -1;
    double lastTick = 0.0, pendingDt = 0.0, lampLevel = 0.0;
    int lastBlocks = -1;
    wcomp::needle::Gr grNeedle;                           // the three meters run all the time; the needle shows one
    wcomp::needle::Io inNeedle, outNeedle;
    wcomp::needle::Lamp lamp;
};
