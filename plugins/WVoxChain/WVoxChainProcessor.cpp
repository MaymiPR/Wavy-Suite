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

void WVoxChainProcessor::prepareToPlay (double sampleRate, int block)
{
    sr = sampleRate;
    setLatencySamples (wvoxchain::latencySamples (sr));
    chain.prepare (sr, jmax (block, 16));
    work.setSize (2, jmax (block, 16));
}

void WVoxChainProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), ins = getTotalNumInputChannels(), outs = b.getNumChannels();
    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);
    float pin[2] { 0.0f, 0.0f }, pout[2] { 0.0f, 0.0f };
    for (int c = 0; c < 2; ++c)                                         // (Mono/Stereo: the input on both sides)
    {
        work.copyFrom (c, 0, b, jmin (c, jmax (0, ins - 1)), 0, n);
        pin[c] = work.getMagnitude (c, 0, n);
    }
    wvoxchain::dsp::Params p;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) p.bpm = *bpm;
    p.sensitivity = get (wvoxchain::sensitivity); p.output = get (wvoxchain::output); p.balance = get (wvoxchain::balance);
    for (size_t i = 0; i < 6; ++i)
    {
        const auto& st = wvoxchain::stages()[i];
        p.fader[i] = get (st.fader);
        p.colour[i] = roundToInt (get (st.colour));
    }
    chain.process (work.getWritePointer (0), work.getWritePointer (1), n, p);
    for (int c = 0; c < outs; ++c)
    {
        b.copyFrom (c, 0, work, outs == 1 ? 0 : jmin (c, 1), 0, n);
        if (outs == 1) { b.addFrom (0, 0, work, 1, 0, n); b.applyGain (0, 0, n, 0.5f); }
    }
    for (int c = 0; c < 2; ++c) pout[c] = work.getMagnitude (c, 0, n);
    auto db = [] (float v) { return v > 1.0e-5f ? Decibels::gainToDecibels (v) : -100.0f; };
    inL.push (db (pin[0])); inR.push (db (pin[1]));
    outL.push (db (pout[0])); outR.push (db (pout[1]));
    sensLevelDb.store (chain.levelDb());
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
