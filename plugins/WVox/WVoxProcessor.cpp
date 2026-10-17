#include "WVoxProcessor.h"
#include "WVoxEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout WVoxProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto db = [] (float v) { return String (v, 1) + " dB"; };
    auto dbParse = [] (const String& s) { return -std::abs (s.getFloatValue()); };
    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, bool visible = true)
    {
        AudioParameterFloatAttributes a;
        a = a.withStringFromValueFunction ([db] (float v, int) { return db (v); })
             .withValueFromStringFunction (dbParse)
             .withAutomatable (visible);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    add (wvox::comp, "Comp", { -36.0f, 0.0f, 0.1f }, 0.0f);
    add (wvox::gate, "Gate", { -80.0f, 0.0f, 0.1f }, -80.0f);
    add (wvox::gain, "Gain", { -36.0f, 0.0f, 0.1f }, 0.0f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { wvox::gateRel, 1 }, "Gate Release",
                                                  NormalisableRange<float> { 10.0f, 250.0f, 0.1f }, 130.0f,
                                                  AudioParameterFloatAttributes().withAutomatable (false)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wvox::mode, 1 }, "Mode", StringArray { "Stereo", "Mono" }, 0));
    return l;
}

WVoxProcessor::WVoxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "WVox", layout())
{
}

bool WVoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

// Phase 2 (UI): audio passes through unchanged. Only the meters are live. The compressor,
// gate and limiter are implemented after the measurements of Phase 3.
void WVoxProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), ch = b.getNumChannels();
    auto toDb = [] (float v) { return v > 1.0e-4f ? Decibels::gainToDecibels (v) : -80.0f; };
    float peakL = b.getMagnitude (0, 0, n);
    float peakR = ch > 1 ? b.getMagnitude (1, 0, n) : peakL;
    const float in = juce::jmax (peakL, peakR);
    auto smooth = [] (std::atomic<float>& a, float v)
    {
        const float old = a.load();
        a.store (v > old ? v : old + (v - old) * 0.15f);   // quick rise, slow fall
    };
    smooth (energyDb, toDb (in));
    smooth (outLDb, toDb (peakL));
    smooth (outRDb, toDb (peakR));
    attnDb.store (0.0f);
}

AudioProcessorEditor* WVoxProcessor::createEditor() { return new WVoxEditor (*this); }

void WVoxProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WVoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WVoxProcessor(); }
