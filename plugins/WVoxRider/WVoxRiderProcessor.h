#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyMeter.h"
#include "WavyPresets.h"
#include "WVoxRiderRide.h"

// Parameters follow the reference's (Vocal Rider's VST3 parameters and ParamXML ranges, defaults and resolutions; see
// RESEARCH.md, WVox Rider). No latency (as the reference's); a side-chain input for the music. The riding is the model
// identified on the reference's audio (WVoxRiderRide.h; MEASUREMENTS.md, WVox Rider Phase 3 / 4). The music does not
// steer it yet (the reference's use of it could not be measured outside a host that activates its side-chain).
namespace wvoxrider
{
inline constexpr const char* target = "target";                    // -42 .. 0 dB, default -21 (the Target slider)
inline constexpr const char* vocalSensitivity = "vocalSensitivity"; // -12 .. +12 dB, default 0
inline constexpr const char* musicSensitivity = "musicSensitivity"; // -12.1 (Off) .. +12 dB, default 0
inline constexpr const char* rangeMax = "rangeMax";                // -11 .. +12 dB, default +6
inline constexpr const char* idle = "idle";                        // 0.02 .. 0.98 of the range (the Idle arrow), default 0.5
inline constexpr const char* rangeMin = "rangeMin";                // -12 .. +11 dB, default -6
inline constexpr const char* rider = "rider";                      // -12 .. +12 dB: the Rider fader (written / read by Automation)
inline constexpr const char* output = "output";                    // -12 .. +12 dB, default 0
inline constexpr const char* attack = "attack";                    // Slow / Fast
inline constexpr const char* spill = "spill";                      // -48 .. -12 dB, default -48 (Live components)
inline constexpr const char* automation = "automation";            // Off / Write / Read
inline constexpr const char* mode = "mode";                        // Stereo, Mono, Live Stereo, Live Mono (the four components)

inline const juce::StringArray& modeNames() { static const juce::StringArray m { "Stereo", "Mono", "Live Stereo", "Live Mono" }; return m; }
inline bool isLive (int modeIndex) { return modeIndex >= 2; }
inline bool musicOff (float value) { return value < -12.05f; }
// the Idle arrow's value in dB: its place in the range
inline float idleDb (float fraction, float lo, float hi) { return lo + fraction * (hi - lo); }
enum Automation { Off, Write, Read };
}
#include "WVoxRiderPresets.h"

class WVoxRiderProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    WVoxRiderProcessor();
    ~WVoxRiderProcessor() override { stopTimer(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WVox Rider"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1 + (int) wvoxrider::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wvoxrider::factoryPresets()[(size_t) index - 1], apvts.getParameter (wvoxrider::mode));
        readExactIdle();
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wvoxrider::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };
    juce::AudioProcessorValueTreeState apvts;

    // editor taps: the vocal level (dB, the highest of the block, for the meter under Target), the output's peak (clip
    // LED), the music level (the Music LED), the Rider's gain (dB), vocal activity (the Ride / Idle LEDs)
    wavy::PeakTap vocalDb, outDb, musicDb;
    std::atomic<float> rideDb { 0.0f };
    std::atomic<bool> riding { false }, sideChainOn { false };
    // the Rider fader grabbed in the editor: its gain while held (riding resumes on release)
    std::atomic<bool> userRide { false };
    std::atomic<float> userRideDb { 0.0f };

    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int modeIndex() const { return juce::roundToInt (get (wvoxrider::mode)); }
    int automationMode() const { return juce::roundToInt (get (wvoxrider::automation)); }
    // the Idle arrow's place: a factory preset's own value (presets use 0 and 1, outside the arrow's 0.02 .. 0.98) while
    // the arrow stays where the preset put it, else the parameter's
    float idleFraction() const
    {
        const float v = get (wvoxrider::idle), e = exactIdle.load();
        return (e >= 0.0f && std::abs (v - juce::jlimit (0.02f, 0.98f, e)) < 1.0e-4f) ? e : v;
    }
    float idleDb() const { return wvoxrider::idleDb (idleFraction(), get (wvoxrider::rangeMin), get (wvoxrider::rangeMax)); }
    void readExactIdle();                                       // (after a preset: also called by the timer)

private:
    juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;                              // Automation Write: the ride to the host; the preset's Idle
    std::atomic<float> exactIdle { -1.0f };
    double sr = 48000.0;
    wvoxrider::Ride ride;
    double outGain = 1.0, heldDb = 0.0, smooth5 = 0.0;          // the Output gain and a held Rider, each smoothed over 5 ms
    double musicLevel = 1.0e-10;
    bool wasHeld = false, returning = false, writing = false;
    int currentProgram = 0;
};
