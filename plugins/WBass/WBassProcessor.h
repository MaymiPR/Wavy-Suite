#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyMeter.h"
#include "WBassPresets.h"
#include "WBassDsp.h"

// Parameters follow the reference's parameter list (ParamXML order, ranges and defaults; see RESEARCH.md, WBass).
namespace wbass
{
inline constexpr const char* inGain = "inGain";        // -30..0 dB, hidden, default 0
inline constexpr const char* freq = "freq";            // 32..256 Hz, logarithmic, default 80
inline constexpr const char* response = "response";    // 0..30 ms, hidden, default 30
inline constexpr const char* ratio = "ratio";          // 0.25..1, hidden, default 0.63
inline constexpr const char* intensity = "intensity";  // -24..+24 dB, default 0
inline constexpr const char* origBass = "origBass";    // Original Bass In (true) / Out (false)
inline constexpr const char* gain = "gain";            // -30..0 dB output, default 0
inline constexpr const char* decay = "decay";          // 0..1, hidden, default 1
inline constexpr const char* smoothTc = "smoothTc";    // 0..1000, hidden, default 20
inline constexpr const char* tc = "tc";                // 0..2000 ms, hidden, default 20
inline constexpr const char* xfadeTc = "xfadeTc";      // 0..5000, hidden, default 10
inline constexpr const char* smooth = "smooth";        // on / off, hidden, default on
inline constexpr const char* mode = "mode";            // 0 Stereo, 1 Mono
}

class WBassProcessor : public juce::AudioProcessor
{
public:
    WBassProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WBass"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    // programs for the host's preset list: 0 = Default, then the factory presets (wbass::factoryPresets()).
    // A change applies only when the number changes, so a host that sets program 0 at load keeps the restored state.
    int getNumPrograms() override { return 1 + (int) wbass::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wbass::factoryPresets()[(size_t) index - 1], apvts.getParameter (wbass::mode));
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wbass::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };   // the reference undoes the last 32 actions
    juce::AudioProcessorValueTreeState apvts;

    // meters (dB, highest peak since the editor last read them): original bass below Freq, created harmonics, output
    // peaks; clip latch
    wavy::PeakTap origDb, harmDb, outLDb, outRDb;
    std::atomic<bool> clipped { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    int currentProgram = 0;
    wbass::Processor dsp;
};
