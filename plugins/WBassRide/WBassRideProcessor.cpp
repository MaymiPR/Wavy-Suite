#include "WBassRideProcessor.h"
#include "WBassRideEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout WBassRideProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float)> text)
    {
        auto a = AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); });
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    auto db1 = [] (float v) { return String (v, 1) + " dB"; };
    auto plain1 = [] (float v) { return String (v, 1); };
    addF (wbassride::target, "Target", { -42.0f, 0.0f, 0.1f }, -14.0f, db1);
    addF (wbassride::sensitivity, "Sensitivity", { 0.0f, 100.0f, 0.1f }, 50.0f, plain1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wbassride::response, 1 }, "Response", StringArray { "Slow", "Fast" }, 0));
    addF (wbassride::spill, "Spill", { -50.0f, -3.0f, 0.1f }, -40.0f, db1);
    addF (wbassride::artifacts, "Artifacts", { 0.0f, 100.0f, 0.1f }, 50.0f, plain1);
    addF (wbassride::rangeMax, "Range Max", { -11.0f, 12.0f, 0.1f }, 12.0f, db1);
    addF (wbassride::rangeMin, "Range Min", { -12.0f, 11.0f, 0.1f }, -12.0f, db1);
    // the Idle arrow is stored as its place in the range (as the reference's preset arrays); the host sees it in dB
    // over a fixed +-12 dB (as the reference's: its text stays 0.0 at 0.5 whatever the range)
    addF (wbassride::idle, "Idle", { 0.02f, 0.98f, 0.01f }, 0.5f, [] (float v) { return String (wbassride::idleDb (v, -12.0f, 12.0f), 1); });
    addF (wbassride::output, "Output", { -12.0f, 12.0f, 0.1f }, 0.0f, db1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wbassride::mode, 1 }, "Mode", wbassride::modeNames(), 0));
    return l;
}

WBassRideProcessor::WBassRideProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "WBassRide", layout())
{
    setLatencySamples (roundToInt (wbassride::latencyMs (0) * 0.001 * sr));
}

bool WBassRideProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void WBassRideProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    configuredMode = -1;
    configure();
}

void WBassRideProcessor::configure()
{
    configuredMode = modeIndex();
    delaySamples = roundToInt (wbassride::latencyMs (configuredMode) * 0.001 * sr);
    line.assign ((size_t) jmax (1, getMainBusNumInputChannels()), std::vector<float> ((size_t) delaySamples + 1, 0.0f));
    writePos = 0;
    ride.prepare (sr);
    setLatencySamples (delaySamples);
}

void WBassRideProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    if (modeIndex() != configuredMode || (int) line.size() < b.getNumChannels()) configure();
    const int n = b.getNumSamples(), ch = jmin (b.getNumChannels(), (int) line.size());
    const double target = get (wbassride::target), lo = get (wbassride::rangeMin), hi = get (wbassride::rangeMax);
    const double idle = idleDb(), gate = get (wbassride::spill), outDb = get (wbassride::output);
    const bool held = userRide.load();

    // the provisional riding model (WBassRideProvisional.h) listens to the input as it comes in; its gain is applied
    // to the audio the component's latency later, so a note's gain is in place when the note is heard (as the
    // reference's look-ahead)
    const int len = delaySamples + 1;
    int pos = writePos;
    float energyMax = -100.0f;
    for (int i = 0; i < n; ++i)
    {
        float in[2] = { b.getSample (0, i), ch > 1 ? b.getSample (1, i) : 0.0f };
        if (ride.push (in, ch, target, lo, hi, idle, gate))
        {
            energyMax = jmax (energyMax, (float) ride.energy);
            gainTo = (float) Decibels::decibelsToGain ((held ? (double) userRideDb.load() : ride.gain) + outDb);
            gainStep = (gainTo - gainNow) / (float) jmax (1, (int) std::lround (sr / 1000.0));
        }
        if (gainStep != 0.0f)
        {
            gainNow += gainStep;
            if ((gainStep > 0.0f && gainNow >= gainTo) || (gainStep < 0.0f && gainNow <= gainTo)) { gainNow = gainTo; gainStep = 0.0f; }
        }
        const int rd = pos - delaySamples < 0 ? pos - delaySamples + len : pos - delaySamples;
        for (int c = 0; c < ch; ++c)
        {
            auto& d = line[(size_t) c];
            d[(size_t) pos] = b.getSample (c, i);
            b.setSample (c, i, d[(size_t) rd] * gainNow);
        }
        pos = pos + 1 == len ? 0 : pos + 1;
    }
    writePos = pos;
    energyDb.push (energyMax);
    rideDb.store (held ? userRideDb.load() : (float) ride.gain);
    const float pk = b.getMagnitude (0, n);
    this->outDb.push (pk > 1.0e-5f ? Decibels::gainToDecibels (pk) : -100.0f);
}

AudioProcessorEditor* WBassRideProcessor::createEditor() { return new WBassRideEditor (*this); }

void WBassRideProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WBassRideProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WBassRideProcessor(); }
