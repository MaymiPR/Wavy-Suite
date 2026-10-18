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

inline void applyPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset, juce::RangedAudioParameter* keep)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || rp == keep) continue;
        float target = rp->getDefaultValue();
        for (const auto& v : preset.values)
            if (rp->getParameterID() == v.id) target = rp->convertTo0to1 (v.value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (target);
        rp->endChangeGesture();
    }
}
}
