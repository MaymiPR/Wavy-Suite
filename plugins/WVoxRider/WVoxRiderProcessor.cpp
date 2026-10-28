#include "WVoxRiderProcessor.h"
#include "WVoxRiderEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout WVoxRiderProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float)> text,
                     bool automatable = true)
    {
        auto a = AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); })
                                                .withAutomatable (automatable);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    auto db1 = [] (float v) { return String (v, 1) + " dB"; };
    // in the reference's parameter order
    addF (wvoxrider::target, "Target", { -42.0f, 0.0f, 0.1f }, -21.0f, db1);
    addF (wvoxrider::vocalSensitivity, "Vocal Sensitivity", { -12.0f, 12.0f, 0.1f }, 0.0f, db1);
    addF (wvoxrider::musicSensitivity, "Music Sensitivity", { -12.1f, 12.0f, 0.1f }, 0.0f,
          [] (float v) { return wvoxrider::musicOff (v) ? String ("Off") : String (v, 1) + " dB"; });
    addF (wvoxrider::rangeMax, "Range Max", { -11.0f, 12.0f, 0.1f }, 6.0f, db1);
    // the Idle arrow is stored as its place in the range (as the reference's preset arrays); the host sees it in dB
    // over a fixed +-12 dB (as the reference's: its text stays 0.0 at 0.5 whatever the range)
    addF (wvoxrider::idle, "Idle", { 0.02f, 0.98f, 0.01f }, 0.5f, [] (float v) { return String (wvoxrider::idleDb (v, -12.0f, 12.0f), 1); });
    addF (wvoxrider::rangeMin, "Range Min", { -12.0f, 11.0f, 0.1f }, -6.0f, db1);
    addF (wvoxrider::rider, "Rider Fader", { -12.0f, 12.0f, 0.1f }, 0.0f, db1);
    addF (wvoxrider::output, "Output", { -12.0f, 12.0f, 0.1f }, 0.0f, db1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wvoxrider::attack, 1 }, "Attack", StringArray { "Slow", "Fast" }, 0));
    addF (wvoxrider::spill, "Spill", { -48.0f, -12.0f, 0.1f }, -48.0f, db1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wvoxrider::automation, 1 }, "Automation", StringArray { "Off", "Write", "Read" }, 0,
                                                   AudioParameterChoiceAttributes().withAutomatable (false)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { wvoxrider::mode, 1 }, "Mode", wvoxrider::modeNames(), 0));
    return l;
}

WVoxRiderProcessor::WVoxRiderProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "WVoxRider", layout())
{
    startTimerHz (30);
}

bool WVoxRiderProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out || (in != AudioChannelSet::mono() && in != AudioChannelSet::stereo())) return false;
    if (l.inputBuses.size() < 2) return true;
    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == AudioChannelSet::mono() || sc == AudioChannelSet::stereo();
}

void WVoxRiderProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    ride.prepare (sr);
    smooth5 = std::exp (-1.0 / (0.005 * sr));
    outGain = Decibels::decibelsToGain ((double) get (wvoxrider::output));
    heldDb = 0.0;
    musicLevel = 1.0e-10;
}

void WVoxRiderProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    auto b = getBusBuffer (buffer, true, 0);
    const int n = b.getNumSamples(), ch = b.getNumChannels();
    wvoxrider::RideSettings st;
    st.target = get (wvoxrider::target); st.rangeMin = get (wvoxrider::rangeMin); st.rangeMax = get (wvoxrider::rangeMax);
    st.idleDb = idleDb(); st.vocalSensitivity = get (wvoxrider::vocalSensitivity); st.spill = get (wvoxrider::spill);
    st.fast = get (wvoxrider::attack) > 0.5f; st.live = wvoxrider::isLive (modeIndex());
    const double outTo = Decibels::decibelsToGain ((double) get (wvoxrider::output));
    const int automation = automationMode();
    const bool held = userRide.load();
    const double heldTo = held ? (double) userRideDb.load() : (double) get (wvoxrider::rider);
    const bool fixed = held || automation == wvoxrider::Read;   // a hand on the fader, or the host's automation
    if (! fixed && wasHeld) { returning = true; wasHeld = false; }

    // the music on the side-chain (its level for the Music LED; the riding does not use it yet)
    const bool sc = getBusCount (true) > 1 && getChannelCountOfBus (true, 1) > 0;
    sideChainOn.store (sc);
    if (sc)
    {
        auto m = getBusBuffer (buffer, true, 1);
        const double k = std::exp (-1.0 / (0.105 * sr));
        for (int i = 0; i < n; ++i)
        {
            double p = 0.0;
            for (int c = 0; c < m.getNumChannels(); ++c) p += (double) m.getSample (c, i) * m.getSample (c, i);
            musicLevel = p / m.getNumChannels() + k * (musicLevel - p / m.getNumChannels());
        }
        musicDb.push ((float) (10.0 * std::log10 (musicLevel + 1.0e-12)));
    }

    float vocalMax = -100.0f;
    for (int i = 0; i < n; ++i)
    {
        const double mid = ch > 1 ? 0.5 * ((double) b.getSample (0, i) + (double) b.getSample (1, i)) : (double) b.getSample (0, i);
        const double r = ride.push (mid, st);
        vocalMax = jmax (vocalMax, (float) ride.levelDb());
        // a held Rider (or Automation Read): its gain, reached over 5 ms; on release the ride is rejoined over 5 ms
        if (fixed || returning)
        {
            const double to = fixed ? heldTo : r;
            heldDb = to + smooth5 * (heldDb - to);
            returning = ! fixed && std::abs (heldDb - r) > 0.005;
            wasHeld = wasHeld || fixed;
        }
        else heldDb = r;
        outGain = outTo + smooth5 * (outGain - outTo);
        const float g = (float) (Decibels::decibelsToGain (heldDb) * outGain);
        for (int c = 0; c < ch; ++c) b.setSample (c, i, b.getSample (c, i) * g);
    }
    vocalDb.push (vocalMax);
    rideDb.store ((float) heldDb);
    riding.store (ride.detecting());
    const float pk = b.getMagnitude (0, n);
    outDb.push (pk > 1.0e-5f ? Decibels::gainToDecibels (pk) : -100.0f);
}

// a factory preset's own Idle value (wavy::presetExactTag in the state)
void WVoxRiderProcessor::readExactIdle()
{
    const auto t = apvts.state.getChildWithName (wavy::presetExactTag);
    exactIdle.store (t.isValid() && t.hasProperty (wvoxrider::idle) ? (float) t.getProperty (wvoxrider::idle) : -1.0f);
}

// Automation Write: the Rider's movements go to the host as if a hand moved its parameter (one gesture while writing)
void WVoxRiderProcessor::timerCallback()
{
    readExactIdle();
    auto* p = apvts.getParameter (wvoxrider::rider);
    const bool write = automationMode() == wvoxrider::Write;
    if (write != writing)
    {
        writing = write;
        if (write) p->beginChangeGesture(); else p->endChangeGesture();
    }
    if (write)
    {
        const float v = p->convertTo0to1 (jlimit (-12.0f, 12.0f, rideDb.load()));
        if (std::abs (v - p->getValue()) > 1.0e-4f) p->setValueNotifyingHost (v);
    }
}

AudioProcessorEditor* WVoxRiderProcessor::createEditor() { return new WVoxRiderEditor (*this); }

void WVoxRiderProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WVoxRiderProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) { apvts.replaceState (ValueTree::fromXml (*xml)); readExactIdle(); }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WVoxRiderProcessor(); }
