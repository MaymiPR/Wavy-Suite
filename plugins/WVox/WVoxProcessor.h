#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyMeter.h"
#include "WVoxPresets.h"
#include "WVoxDsp.h"

// Parameters follow the reference plugin's parameter list (ranges and defaults from its ParamXML).
namespace wvox
{
inline constexpr const char* comp = "comp";          // -36..0 dB, default 0
inline constexpr const char* gate = "gate";          // -80..0 dB, default -80 (off)
inline constexpr const char* gateRel = "gateRel";    // 10..250 ms, hidden, default 130
inline constexpr const char* gain = "gain";          // -36..0 dB, default 0 (limiter ceiling)
inline constexpr const char* mode = "mode";          // 0 Stereo, 1 Mono
}

class WVoxProcessor : public juce::AudioProcessor
{
public:
    WVoxProcessor();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WVox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    // programs for the host's preset list: 0 = Default, then the factory presets (wvox::factoryPresets()).
    // A change applies only when the number changes, so a host that sets program 0 at load keeps the restored state.
    int getNumPrograms() override { return 1 + (int) wvox::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wvox::factoryPresets()[(size_t) index - 1], apvts.getParameter (wvox::mode));
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wvox::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };   // the reference undoes the last 32 actions
    juce::AudioProcessorValueTreeState apvts;

    // Meter values for the editor, the highest since it last read them (dB): input energy, depth of the total gain
    // reduction (>= 0), output peaks.
    wavy::PeakTap energyDb, attnDepth, outLDb, outRDb;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    int currentProgram = 0;
    void configure();

    wvox::Processor procs[2];
    std::atomic<float>* pComp = nullptr; std::atomic<float>* pGate = nullptr; std::atomic<float>* pGain = nullptr; std::atomic<float>* pMode = nullptr;
    double sr = 48000.0;
    bool prepared = false, layoutDual = false;
};
