#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "WavyMeter.h"
#include "WDelayPresets.h"
#include "WDelayDsp.h"
#include "WCompAnalog.h"
#include "WDelayAnalogTables.h"

// Parameters follow the reference's parameter list (ParamXML order, ranges, tapers and defaults; see RESEARCH.md,
// WDelay). The DSP (WDelayDsp.h) and the Analog stage as identified on the reference (MEASUREMENTS.md).
namespace wdelay
{
inline constexpr const char* delay = "delay";          // 1 .. 3500 ms (the reference's taper), default 375
inline constexpr const char* delayNote = "delayNote";  // 20 note values (1/64T .. 2 bars), default 1/8D
inline constexpr const char* delayR = "delayR";        // the right side's time and note (Dual)
inline constexpr const char* delayNoteR = "delayNoteR";
inline constexpr const char* sync = "sync";            // BPM / Host / ms, default Host
inline constexpr const char* bpm = "bpm";              // 30 .. 300, default 120 (used in BPM sync)
inline constexpr const char* feedback = "feedback";    // 0 .. 200 %, default 60
inline constexpr const char* mix = "mix";              // 0 (dry) .. 100 (wet), default 100
inline constexpr const char* output = "output";        // -18 .. +18 dB, default 0
inline constexpr const char* analog = "analog";        // Off / 1 .. 4, default 2
inline constexpr const char* lofi = "lofi";            // Off / On
inline constexpr const char* rate = "rate";            // 0.1 .. 6000 Hz, logarithmic, default 0.1
inline constexpr const char* depth = "depth";          // 0 .. 100 %, default 0
inline constexpr const char* hipass = "hipass";        // 19 (Off) .. 20 139 Hz, logarithmic
inline constexpr const char* lopass = "lopass";        // 19 .. 20 139 Hz (Off from 20 100), logarithmic
inline constexpr const char* phaseL = "phaseL";        // the left (mono: the only) delay line inverted
inline constexpr const char* phaseR = "phaseR";
inline constexpr const char* pingPong = "pingPong";
inline constexpr const char* pingPongStart = "pingPongStart";   // Left / Right
inline constexpr const char* dual = "dual";            // separate left and right times
inline constexpr const char* mode = "mode";            // 0 Stereo, 1 Mono, 2 mono in, stereo out

constexpr float kHiPassOff = 19.5f, kLoPassOff = 20100.0f;
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T",
                                       "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1B", "2B" };
    return n;
}
// a note value's length in beats (quarter notes)
double noteBeats (int note);
// the note value closest to a time at a tempo (used when the display changes from ms to note values)
int nearestNote (double ms, double tempo);
// the reference's Delay taper: 1 ms per 1/1000 of the range up to 501 ms at the middle, then the measured curve to 3500
float delayFromNorm (float n);
float delayToNorm (float ms);
juce::String freqText (float hz, bool hiPass);
// a parameter's value as the reference uses it (rounded to its resolution)
float atResolution (const char* id, float value);
juce::String rateText (float hz);
}

class WDelayProcessor : public juce::AudioProcessor
{
public:
    WDelayProcessor();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WDelay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1 + (int) wdelay::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index == currentProgram || index < 0 || index >= getNumPrograms()) return;
        currentProgram = index;
        static const wavy::FactoryPreset defaults { "", "Default", {} };
        wavy::applyPreset (apvts, index == 0 ? defaults : wdelay::factoryPresets()[(size_t) index - 1], apvts.getParameter (wdelay::mode),
                           { apvts.getParameter (wdelay::mix) });           // presets leave Mix alone, as the reference
    }
    const juce::String getProgramName (int index) override
    {
        if (index <= 0 || index >= getNumPrograms()) return "Default";
        const auto& p = wdelay::factoryPresets()[(size_t) index - 1];
        return juce::String (p.group) + ": " + p.name;
    }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 32, 32 };
    juce::AudioProcessorValueTreeState apvts;

    // meters (dB, highest peak since the editor last read them): the output, left and right
    wavy::PeakTap outLDb, outRDb;
    std::atomic<int> blocks { 0 };
    std::atomic<double> hostBpm { 0.0 };                  // 0: the host gives no tempo

    // the DSP's settings from the parameters (the times in samples at the tempo the notes follow)
    wdelay::dsp::Settings settings (double sampleRate) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void configure();
    wdelay::dsp::Delay delays[2];                 // one linked delay, or two mono ones (Mode Mono on a stereo bus)
    wcomp::Analog analogStage;
    double sr = 48000.0;
    bool prepared = false, layoutDual = false;
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int currentProgram = 0;
};
