#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyPresets.h"
#include "WavyMeter.h"

// WVox Chain: a one-fader-per-stage vocal chain modelled on CLA Vocals (RESEARCH.md, WVox Chain): Sensitivity, Bass,
// Treble, Compress, Reverb, Delay and Pitch faders, each stage with a colour, Output and Balance. Phase 2: the
// parameters as the reference's, its latency and the editor; the audio passes at the Sensitivity and Output gains
// until the chain's DSP (Phase 4).
namespace wvoxchain
{
inline constexpr const char* sensitivity = "sensitivity";
inline constexpr const char* bass = "bass";
inline constexpr const char* bassColor = "bassColor";
inline constexpr const char* treble = "treble";
inline constexpr const char* trebleColor = "trebleColor";
inline constexpr const char* compress = "compress";
inline constexpr const char* compressColor = "compressColor";
inline constexpr const char* reverb = "reverb";
inline constexpr const char* reverbColor = "reverbColor";
inline constexpr const char* delay = "delay";
inline constexpr const char* delayColor = "delayColor";
inline constexpr const char* pitch = "pitch";
inline constexpr const char* pitchColor = "pitchColor";
inline constexpr const char* output = "output";
inline constexpr const char* balance = "balance";
inline constexpr const char* mode = "mode";                        // Stereo, Mono/Stereo (the two components)

// the six stages: fader and colour IDs, the label under the fader, the colours' names (the host's texts, the clear
// position first)
struct Stage { const char* fader; const char* colour; const char* label; const char* names[4]; };
inline const std::array<Stage, 6>& stages()
{
    static const std::array<Stage, 6> s { {
        { bass, bassColor, "BASS", { "Bypass", "Sub", "Lower", "Upper" } },
        { treble, trebleColor, "TREBLE", { "Bypass", "Bite", "Top", "Roof" } },
        { compress, compressColor, "COMPRESS", { "Bypass", "Push", "Spank", "Wall" } },
        { reverb, reverbColor, "REVERB", { "Mute", "Tight", "Large", "Chamber" } },
        { delay, delayColor, "DELAY", { "Mute", "Slap", "Eight", "Quarter" } },
        { pitch, pitchColor, "PITCH", { "Mute", "Stereo", "Wide", "Spreader" } } } };
    return s;
}
inline const juce::StringArray& modeNames() { static const juce::StringArray m { "Stereo", "Mono/Stereo" }; return m; }
inline bool isMonoIn (int modeIndex) { return modeIndex == 1; }
// the reference's latency (RefRender): 193 samples at 44.1 and 48 kHz, 385 at 96 kHz (fixed samples, doubled with
// the rate; assumed to go on doubling above)
inline int latencySamples (double fs) { const int k = fs > 144000.0 ? 4 : fs > 72000.0 ? 2 : 1; return 193 * k - (k - 1); }
inline const std::vector<wavy::FactoryPreset>& factoryPresets() { static const std::vector<wavy::FactoryPreset> none; return none; }
}

class WVoxChainProcessor : public juce::AudioProcessor
{
public:
    WVoxChainProcessor();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WVox Chain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };
    juce::AudioProcessorValueTreeState apvts;

    // editor taps: the input and output peaks per channel (dB, the highest of the block) for the meters, and the
    // input's level for the Sensitivity LED
    wavy::PeakTap inL, inR, outL, outR;
    std::atomic<float> sensLevelDb { -100.0f };

    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int modeIndex() const { return juce::roundToInt (get (wvoxchain::mode)); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout layout();
    double sr = 48000.0;
    int latency = 193, writePos = 0;
    std::vector<std::vector<float>> line;                       // the latency's delay line per channel
    double inGain = 1.0, outGain = 1.0, smooth = 0.0;
};
