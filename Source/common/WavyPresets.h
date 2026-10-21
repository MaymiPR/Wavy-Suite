#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace wavy
{
// A factory preset: real-world values for some parameters; every other parameter (except the Mode switch) goes back
// to its default, so a preset always gives the same sound.
struct FactoryPreset
{
    struct Value { const char* id; float value; };
    const char* group;
    const char* name;
    std::vector<Value> values;
};

// keep / alsoKeep: parameters a preset never changes (the Mode switch; WBassX's Input, which the reference marks
// IgnoreOnPresetLoad)
inline void applyPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset, juce::RangedAudioParameter* keep,
                         const std::vector<juce::AudioProcessorParameter*>& alsoKeep = {})
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || rp == keep || std::find (alsoKeep.begin(), alsoKeep.end(), p) != alsoKeep.end()) continue;
        float target = rp->getDefaultValue();
        for (const auto& v : preset.values)
            if (rp->getParameterID() == v.id) target = rp->convertTo0to1 (v.value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (target);
        rp->endChangeGesture();
    }
}
}
