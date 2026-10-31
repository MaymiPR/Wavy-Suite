#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "WavyMeter.h"
#include "WBassRiderPresets.h"
#include "WBassRiderRide.h"

// Parameters follow the reference's (Bass Rider's VST3 parameters and ParamXML ranges, defaults and resolutions; see
// RESEARCH.md, WBass Rider). The audio is delayed by the component's latency (the look-ahead) and ridden by the model
// identified on the reference's audio (WBassRiderRide.h; MEASUREMENTS.md, WBass Rider Phase 3 / 4).
namespace wbassrider
{
inline constexpr const char* target = "target";            // -42 .. 0 dB, default -14 (the Target slider)
inline constexpr const char* sensitivity = "sensitivity";  // 0 .. 100, default 50 (note detection)
inline constexpr const char* response = "response";        // Slow / Fast
inline constexpr const char* spill = "spill";              // -50 .. -3 dB, default -40 (Ignore Spill)
inline constexpr const char* artifacts = "artifacts";      // 0 .. 100, default 50 (Ignore Artifacts; -20 .. -3 dB inside)
inline constexpr const char* rangeMax = "rangeMax";        // -11 .. +12 dB, default +12
inline constexpr const char* rangeMin = "rangeMin";        // -12 .. +11 dB, default -12
inline constexpr const char* idle = "idle";                // 0.02 .. 0.98 of the range (the Idle arrow), default 0.5
inline constexpr const char* output = "output";            // -12 .. +12 dB, default 0
inline constexpr const char* mode = "mode";                // Stereo, Mono, Live Stereo, Live Mono (the four components)

inline const juce::StringArray& modeNames() { static const juce::StringArray m { "Stereo", "Mono", "Live Stereo", "Live Mono" }; return m; }
inline bool isLive (int modeIndex) { return modeIndex >= 2; }
inline double latencyMs (int modeIndex) { return isLive (modeIndex) ? 5.0 : 42.0; }   // measured: 42.00 / 5.00 ms
// the Idle arrow's value in dB: its place in the range
inline float idleDb (float fraction, float lo, float hi) { return lo + fraction * (hi - lo); }
}

class WBassRiderProcessor : public juce::AudioProcessor
{
public:
    WBassRiderProcessor();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WBass Rider"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    // programs for the host's preset list: 0 = Default, then the factory presets (wbassrider::factoryPresets())
    int getNumPrograms() override { return 1 + (int) wbassrider::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wbassrider::factoryPresets()[(size_t) index - 1], apvts.getParameter (wbassrider::mode));
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wbassrider::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };
    juce::AudioProcessorValueTreeState apvts;

    // editor taps: the input's energy (dB, the highest of the block, for the Target meter), the output's peak (clip LED),
    // the Rider's gain (dB)
    wavy::PeakTap energyDb, outDb;
    std::atomic<float> rideDb { 0.0f };
    // the Rider fader grabbed in the editor: its gain while held (riding resumes on release)
    std::atomic<bool> userRide { false };
    std::atomic<float> userRideDb { 0.0f };

    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int modeIndex() const { return juce::roundToInt (get (wbassrider::mode)); }
    float idleDb() const { return wbassrider::idleDb (get (wbassrider::idle), get (wbassrider::rangeMin), get (wbassrider::rangeMax)); }

private:
    int currentProgram = 0;
    juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void configure();
    double sr = 48000.0;
    int delaySamples = 0, writePos = 0, configuredMode = -1;
    std::vector<std::vector<float>> line;
    wbassrider::Ride ride;
    double outGain = 1.0, heldDb = 0.0, smooth5 = 0.0;      // the Output gain and a held Rider, each smoothed over 5 ms
    bool wasHeld = false, returning = false;
};
