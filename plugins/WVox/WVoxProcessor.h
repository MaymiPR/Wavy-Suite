#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

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

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Wavy WVox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    // Meter values shown by the editor (dB): input energy, total gain reduction (<= 0), output peaks.
    std::atomic<float> energyDb { -80.0f }, attnDb { 0.0f }, outLDb { -80.0f }, outRDb { -80.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
