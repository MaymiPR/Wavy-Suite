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
    pComp = apvts.getRawParameterValue (wvox::comp);
    pGate = apvts.getRawParameterValue (wvox::gate);
    pGain = apvts.getRawParameterValue (wvox::gain);
    pMode = apvts.getRawParameterValue (wvox::mode);
}

bool WVoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void WVoxProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    prepared = false;
    configure();
}

// Stereo bus + Mode "Stereo": one linked gain. Stereo bus + Mode "Mono": two independent mono processors. Mono bus: one.
void WVoxProcessor::configure()
{
    const int ch = getTotalNumInputChannels();
    const bool dual = ch > 1 && pMode->load() > 0.5f;
    layoutDual = dual;
    procs[0].prepare (sr, dual ? 1 : ch);
    procs[1].prepare (sr, 1);
    setLatencySamples (procs[0].getLatency());
    prepared = true;
}

void WVoxProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), ch = b.getNumChannels();
    const bool wantDual = ch > 1 && pMode->load() > 0.5f;
    if (! prepared || wantDual != layoutDual) configure();

    // input energy meter
    auto toDb = [] (float v) { return v > 1.0e-4f ? Decibels::gainToDecibels (v) : -80.0f; };
    float inPeak = 0.0f;
    for (int c = 0; c < ch; ++c) inPeak = jmax (inPeak, b.getMagnitude (c, 0, n));

    wvox::Processor::Params p;
    p.compDb = pComp->load(); p.gateDb = pGate->load(); p.gainDb = pGain->load();
    float* const* w = b.getArrayOfWritePointers();
    if (! layoutDual)
    {
        procs[0].setParams (p);
        const float* const* r = w;
        procs[0].process (r, w, n);
        attnDb.store (procs[0].getReductionDb());
    }
    else
    {
        for (int c = 0; c < 2; ++c)
        {
            procs[c].setParams (p);
            const float* in[1] = { w[c] };
            float* out[1] = { w[c] };
            procs[c].process (in, out, n);
        }
        attnDb.store (jmin (procs[0].getReductionDb(), procs[1].getReductionDb()));
    }

    const float peakL = b.getMagnitude (0, 0, n), peakR = ch > 1 ? b.getMagnitude (1, 0, n) : peakL;
    auto smooth = [] (std::atomic<float>& a, float v)
    {
        const float old = a.load();
        a.store (v > old ? v : old + (v - old) * 0.15f);   // quick rise, slow fall
    };
    smooth (energyDb, toDb (inPeak));
    smooth (outLDb, toDb (peakL));
    smooth (outRDb, toDb (peakR));
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
