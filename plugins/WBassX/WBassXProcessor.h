#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyMeter.h"
#include "WBassXPresets.h"

// Parameters follow the reference's parameter list (ParamXML order, ranges, tapers and defaults; see RESEARCH.md,
// WMaxxBass / WBassX). Phase 2: the audio passes through unchanged; the DSP comes in Phase 4.
namespace wbassx
{
inline constexpr const char* input = "input";          // -inf..0 dB (gain fader taper), default 0
inline constexpr const char* freq = "freq";            // 32..256 Hz, logarithmic, default 80
inline constexpr const char* decay = "decay";          // -24..-9 dB per harmonic, default -15
inline constexpr const char* response = "response";    // 10..30 ms, default 20
inline constexpr const char* ratio = "ratio";          // 1..4 (upward compression of the harmonics), logarithmic, default 1
inline constexpr const char* monitor = "monitor";      // Audio / Harmonics / Original Bass
inline constexpr const char* highpass = "highpass";    // 6 / 12 / 24 dB per octave on the harmonics, default 12
inline constexpr const char* origBass = "origBass";    // -inf..+12 dB, default 0
inline constexpr const char* harmonics = "harmonics";  // -inf..+12 dB, default -inf
inline constexpr const char* mode = "mode";            // 0 Stereo, 1 Mono
}

class WBassXProcessor : public juce::AudioProcessor
{
public:
    WBassXProcessor();

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WBassX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    // programs for the host's preset list: 0 = Default, then the factory presets
    int getNumPrograms() override { return 1 + (int) wbassx::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wbassx::factoryPresets()[(size_t) index - 1], apvts.getParameter (wbassx::mode));
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wbassx::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };
    juce::AudioProcessorValueTreeState apvts;

    // output meters (dB, highest peak since the editor last read them) and the clip latch
    wavy::PeakTap outLDb, outRDb;
    std::atomic<bool> clipped { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    int currentProgram = 0;
};
