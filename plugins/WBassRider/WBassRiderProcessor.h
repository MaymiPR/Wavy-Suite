#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "WavyMeter.h"
#include "WavyPresets.h"
#include "WBassRiderProvisional.h"

// Parameters follow the reference's (Bass Rider's VST3 parameters and ParamXML ranges, defaults and resolutions; see
// RESEARCH.md, WBass Rider). Phase 2: the audio is delayed by the component's latency and ridden by a provisional model
// fitted on films of the reference's editor (WBassRiderProvisional.h), until the riding DSP (Phase 4).
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

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WBass Rider"; }
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
    juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void configure();
    double sr = 48000.0;
    int delaySamples = 0, writePos = 0, configuredMode = -1;
    std::vector<std::vector<float>> line;
    wbassrider::ProvisionalRide ride;
    float gainNow = 1.0f, gainTo = 1.0f, gainStep = 0.0f;
};
