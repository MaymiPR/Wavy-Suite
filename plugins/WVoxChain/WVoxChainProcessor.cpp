#include "WVoxChainProcessor.h"
#include "WVoxChainEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout WVoxChainProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto text1 = [] (float v) { return String (v, 1); };
    auto addF = [&] (const char* id, const char* name, float lo, float hi, std::function<String (float)> text, const char* unit = "")
    {
        auto a = AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); }).withLabel (unit);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), 0.0f, a));
    };
    // in the reference's host order: Sensitivity, then each fader and its colour, Output, Balance
    addF (wvoxchain::sensitivity, "Sensitivity", -10.0f, 10.0f, text1, "dB");
    const char* names[6][2] = { { "Bass", "Bass Color" }, { "Treble", "Treble Color" }, { "Compress", "Compress Color" },
                                { "Reverb", "Reverb Color" }, { "Delay", "Delay Color" }, { "Pitch", "Pitch Color" } };
    for (size_t i = 0; i < 6; ++i)
    {
        const auto& s = wvoxchain::stages()[i];
        addF (s.fader, names[i][0], -10.0f, 10.0f, text1);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { s.colour, 1 }, names[i][1],
                                                       StringArray { s.names[0], s.names[1], s.names[2], s.names[3] }, 1));
    }
    addF (wvoxchain::output, "Output", -10.0f, 10.0f, text1, "dB");
    addF (wvoxchain::balance, "Balance", -6.0f, 6.0f, text1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wvoxchain::mode, 1 }, "Mode", wvoxchain::modeNames(), 0));
    return l;
}

WVoxChainProcessor::WVoxChainProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "WVoxChain", layout())
{
    setLatencySamples (wvoxchain::latencySamples (48000.0));
}

// stereo in / out (Stereo), mono in / stereo out (Mono/Stereo); a host offering mono out gets mono
bool WVoxChainProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out == AudioChannelSet::stereo()) return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
    return out == AudioChannelSet::mono() && in == AudioChannelSet::mono();
}

void WVoxChainProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    latency = wvoxchain::latencySamples (sr);
    setLatencySamples (latency);
    line.assign ((size_t) jmax (1, getTotalNumOutputChannels()), std::vector<float> ((size_t) latency + 1, 0.0f));
    writePos = 0;
    smooth = std::exp (-1.0 / (0.005 * sr));
    inGain = Decibels::decibelsToGain ((double) get (wvoxchain::sensitivity));
    outGain = Decibels::decibelsToGain ((double) get (wvoxchain::output));
}

void WVoxChainProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), ins = getTotalNumInputChannels(), outs = jmin (b.getNumChannels(), (int) line.size());
    if (ins == 1 && outs > 1) b.copyFrom (1, 0, b, 0, 0, n);           // Mono/Stereo: the input on both outputs
    const double inTo = Decibels::decibelsToGain ((double) get (wvoxchain::sensitivity));
    const double outTo = Decibels::decibelsToGain ((double) get (wvoxchain::output));
    const float bal = get (wvoxchain::balance);
    const double gl = Decibels::decibelsToGain ((double) jmin (0.0f, -bal)), gr = Decibels::decibelsToGain ((double) jmin (0.0f, bal));
    float pin[2] { 0.0f, 0.0f }, pout[2] { 0.0f, 0.0f }, sens = 0.0f;
    const int len = latency + 1;
    for (int i = 0; i < n; ++i)
    {
        inGain = inTo + smooth * (inGain - inTo);
        outGain = outTo + smooth * (outGain - outTo);
        for (int c = 0; c < outs; ++c)
        {
            const float x = b.getSample (c, i);
            pin[jmin (c, 1)] = jmax (pin[jmin (c, 1)], std::abs (x));
            const float s = (float) (x * inGain);
            sens = jmax (sens, std::abs (s));
            auto& d = line[(size_t) c];
            d[(size_t) writePos] = s;
            const float y = (float) (d[(size_t) ((writePos + 1) % len)] * outGain * (outs > 1 ? (c == 0 ? gl : gr) : 1.0));
            b.setSample (c, i, y);
            pout[jmin (c, 1)] = jmax (pout[jmin (c, 1)], std::abs (y));
        }
        writePos = (writePos + 1) % len;
    }
    auto db = [] (float v) { return v > 1.0e-5f ? Decibels::gainToDecibels (v) : -100.0f; };
    inL.push (db (pin[0])); inR.push (db (outs > 1 && ins > 1 ? pin[1] : pin[0]));
    outL.push (db (pout[0])); outR.push (db (outs > 1 ? pout[1] : pout[0]));
    sensLevelDb.store (db (sens));
}

AudioProcessorEditor* WVoxChainProcessor::createEditor() { return new WVoxChainEditor (*this); }

void WVoxChainProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WVoxChainProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WVoxChainProcessor(); }
