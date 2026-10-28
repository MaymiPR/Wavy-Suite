#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
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
    std::vector<const char*> leave {};   // parameters this preset leaves where they are (WBass Rider's Target, "*" in the reference)
};

inline const juce::Identifier presetExactTag { "PresetExact" };

// keep / alsoKeep: parameters a preset never changes (the Mode switch; WBassX's Input, which the reference marks
// IgnoreOnPresetLoad)
inline void applyPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset, juce::RangedAudioParameter* keep,
                         const std::vector<juce::AudioProcessorParameter*>& alsoKeep = {})
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || rp == keep || std::find (alsoKeep.begin(), alsoKeep.end(), p) != alsoKeep.end()) continue;
        if (std::any_of (preset.leave.begin(), preset.leave.end(), [rp] (const char* id) { return rp->getParameterID() == id; })) continue;
        float target = rp->getDefaultValue();
        for (const auto& v : preset.values)
            if (rp->getParameterID() == v.id) target = rp->convertTo0to1 (v.value);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (target);
        rp->endChangeGesture();
    }
    // the preset's values as stored, before the parameters' ranges and snapping (a plug-in may use them while its
    // parameters stay where the preset put them: WDelay, as the reference takes a preset's values exactly)
    auto old = apvts.state.getChildWithName (presetExactTag);
    if (old.isValid()) apvts.state.removeChild (old, nullptr);
    juce::ValueTree exact (presetExactTag);
    for (const auto& v : preset.values) exact.setProperty (v.id, v.value, nullptr);
    apvts.state.appendChild (exact, nullptr);
}
}
