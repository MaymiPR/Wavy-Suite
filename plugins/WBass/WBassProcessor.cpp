#include "WBassProcessor.h"
#include "WBassEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout WBassProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, bool automatable, const char* unit, int decimals)
    {
        auto a = AudioParameterFloatAttributes()
                     .withStringFromValueFunction ([unit, decimals] (float v, int) { return String (v, decimals) + (unit[0] ? String (" ") + unit : String()); })
                     .withAutomatable (automatable);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    // on / off switches as two-choice parameters: unlike AudioParameterBool their value always snaps to 0 or 1, so a
    // restored state can never leave them half-way (pluginval's state test)
    auto addB = [&] (const char* id, const char* name, bool def, bool automatable, StringArray labels)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, labels, def ? 1 : 0,
                                                       AudioParameterChoiceAttributes().withAutomatable (automatable)));
    };
    // Freq is logarithmic in the reference (32..256 Hz, 1 Hz steps)
    NormalisableRange<float> freqRange (32.0f, 256.0f,
        [] (float s, float e, float n) { return s * std::pow (e / s, n); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
        [] (float, float, float v) { return std::round (v); });

    addF (wbass::inGain, "Input", { -30.0f, 0.0f, 0.1f }, 0.0f, false, "dB", 1);
    addF (wbass::freq, "Freq", freqRange, 80.0f, true, "Hz", 0);
    addF (wbass::response, "Response", { 0.0f, 30.0f, 0.1f }, 30.0f, false, "ms", 1);
    addF (wbass::ratio, "Ratio", { 0.25f, 1.0f, 0.01f }, 0.63f, false, "", 2);
    addF (wbass::intensity, "Intensity", { -24.0f, 24.0f, 0.1f }, 0.0f, true, "dB", 1);
    addB (wbass::origBass, "Original Bass", true, true, { "Out", "In" });
    addF (wbass::gain, "Gain", { -30.0f, 0.0f, 0.1f }, 0.0f, true, "dB", 1);
    addF (wbass::decay, "Decay", { 0.0f, 1.0f, 0.1f }, 1.0f, false, "", 1);
    addF (wbass::smoothTc, "SmoothTc", { 0.0f, 1000.0f, 0.1f }, 20.0f, false, "", 1);
    addF (wbass::tc, "Tc", { 0.0f, 2000.0f, 0.1f }, 20.0f, false, "ms", 1);
    addF (wbass::xfadeTc, "XFadeTc", { 0.0f, 5000.0f, 0.1f }, 10.0f, false, "", 1);
    addB (wbass::smooth, "Smooth", true, false, { "Off", "On" });
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wbass::mode, 1 }, "Mode", StringArray { "Stereo", "Mono" }, 0));
    return l;
}

WBassProcessor::WBassProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "WBass", layout())
{
}

bool WBassProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void WBassProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate, getTotalNumInputChannels());
}

void WBassProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), ch = jmin (2, b.getNumChannels());
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    dsp.setFreq (get (wbass::freq));
    dsp.setIntensityDb (get (wbass::intensity));
    dsp.setOriginalBass (get (wbass::origBass) > 0.5f);
    dsp.setGainDb (get (wbass::gain));
    dsp.setDecay (get (wbass::decay));
    dsp.process (b.getArrayOfWritePointers(), ch, n);

    auto toDb = [] (float v) { return v > 1.0e-4f ? Decibels::gainToDecibels (v) : -80.0f; };
    const float peakL = b.getMagnitude (0, 0, n), peakR = ch > 1 ? b.getMagnitude (1, 0, n) : peakL;
    auto smooth = [] (std::atomic<float>& a, float v)
    {
        const float old = a.load();
        a.store (v > old ? v : old + (v - old) * 0.15f);   // quick rise, slow fall
    };
    smooth (origDb, toDb (dsp.lastOrigPeak));
    smooth (harmDb, toDb (dsp.lastHarmPeak));
    smooth (outLDb, toDb (peakL));
    smooth (outRDb, toDb (peakR));
    if (jmax (peakL, peakR) >= (float) dsp.t.ceiling) clipped.store (true);
}

AudioProcessorEditor* WBassProcessor::createEditor() { return new WBassEditor (*this); }

void WBassProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WBassProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WBassProcessor(); }
