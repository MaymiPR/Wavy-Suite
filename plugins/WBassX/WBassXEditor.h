#pragma once
#include "WBassXProcessor.h"
#include "WavyRenUI.h"
#include "WavyFlatUI.h"
#include <map>

// WBassX editor: 800 x 395 px design grid (the reference's flat design), resizable at that aspect ratio.
class WBassXEditor : public wavy::ren::Editor
{
public:
    explicit WBassXEditor (WBassXProcessor&);

    static constexpr int W = 800, H = 395;

private:
    void paintContent (juce::Graphics&) override;
    void contentMouseDown (const juce::MouseEvent&) override;
    void contentMouseDrag (const juce::MouseEvent&) override;
    void contentMouseUp (const juce::MouseEvent&) override;
    void setUpGroups();
    void tick() override;
    void paintGraph (juce::Graphics&);
    void paintMeters (juce::Graphics&);
    float value (const char* id) const;

    WBassXProcessor& proc;
    wavy::flat::VFader& input;
    wavy::flat::VFader& origBass;
    wavy::flat::VFader& harmonics;
    wavy::flat::Knob& ratio;
    wavy::flat::Knob& response;
    wavy::flat::Knob& decay;
    wavy::flat::HSlider& freq;
    wavy::flat::SlopeBox& slope;
    wavy::flat::RadioColumn* monitor = nullptr;
    wavy::flat::LassoOverlay& lasso;
    juce::Point<int> lassoStart;
    bool lassoing = false;
    // the controls a lasso can take, in the reference's control order, with their parameters
    struct Member { juce::Component* c; const char* id; float referenceScale; };
    std::vector<Member> groupable;
    std::map<juce::Component*, float> gangStart;          // each member's value when the group drag began
    float gangTotal = 0.0f;

    wavy::flat::Selection selection;
    wavy::MeterBallistics barL, barR;
    double lastTick = 0.0;
    float outL = -100.0f, outR = -100.0f, holdL = -100.0f, holdR = -100.0f;
    bool clip = false;
};
