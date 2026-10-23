#include "WCompProcessor.h"
#include "WCompEditor.h"

using namespace juce;

float wcomp::ratioFromNorm (float v) { return std::exp (4.40703f * v - 0.494827f * v * v); }
float wcomp::ratioToNorm (float r)
{
    // solve 0.494827 v^2 - 4.40703 v + ln r = 0 for the root in 0 .. 1
    const float a = 0.494827f, b = -4.40703f, c = std::log (jlimit (1.0f, 50.0f, r));
    return jlimit (0.0f, 1.0f, (-b - std::sqrt (b * b - 4.0f * a * c)) / (2.0f * a));
}

String wcomp::timeText (float ms)
{
    if (ms < 10.0f) return String (ms, 2) + " ms";
    if (ms < 100.0f) return String (ms, 1) + " ms";
    return String (roundToInt (ms)) + " ms";
}

AudioProcessorValueTreeState::ParameterLayout WCompProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float)> text)
    {
        auto a = AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); });
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    auto addC = [&] (const char* id, const char* name, StringArray items, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto logRange = [] (float lo, float hi, float step)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float s, float e, float n) { return s * std::pow (e / s, n); },
            [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
            [step] (float, float, float v) { return std::round (v / step) * step; });
    };
    const NormalisableRange<float> ratioRange (1.0f, 50.0f,
        [] (float, float, float n) { return wcomp::ratioFromNorm (n); },
        [] (float, float, float v) { return wcomp::ratioToNorm (v); },
        [] (float, float, float v) { return std::round (v * 100.0f) / 100.0f; });

    // as the reference: normalised 0 is 0 dB and 1 is -48 dB (the knob turns clockwise toward -48, dragging up)
    NormalisableRange<float> thr (-48.0f, 0.0f,
        [] (float, float, float n) { return -48.0f * n; },
        [] (float, float, float v) { return jlimit (0.0f, 1.0f, -v / 48.0f); },
        [] (float, float, float v) { return std::round (v * 10.0f) / 10.0f; });
    addF (wcomp::threshold, "Threshold", thr, -18.0f, [] (float v) { return String (v, 1) + " dB"; });
    addF (wcomp::ratio, "Ratio", ratioRange, 3.0f, [] (float v) { return String (v, 2); });
    addF (wcomp::punch, "Punch", { 0.0f, 30.0f, 0.1f }, 1.0f, [] (float v) { return String (v, 1); });
    addF (wcomp::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, [] (float v) { return String (v, 1); });
    addF (wcomp::output, "Output", { -18.0f, 18.0f, 0.1f }, 0.0f, [] (float v) { return String (v, 1) + " dB"; });
    addF (wcomp::attack, "Attack", logRange (0.5f, 100.0f, 0.01f), 7.0f, [] (float v) { return wcomp::timeText (v); });
    addF (wcomp::release, "Release", logRange (3.0f, 3000.0f, 0.01f), 100.25f, [] (float v) { return wcomp::timeText (v); });
    addC (wcomp::releaseNote, "Release Note", { "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D",
                                                "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 Bar", "2 Bars" }, 5);
    addC (wcomp::sync, "Sync", { "BPM", "Host", "ms" }, 2);
    addF (wcomp::bpm, "BPM", { 30.0f, 300.0f, 1.0f }, 120.0f, [] (float v) { return String (roundToInt (v)); });
    addC (wcomp::analog, "Analog", { "Off", "1", "2", "3", "4" }, 2);
    addC (wcomp::limiter, "Limiter", { "Off", "On" }, 0);
    addC (wcomp::meter, "Meter", { "In", "GR", "Out" }, 1);
    addC (wcomp::mode, "Mode", { "Stereo", "Mono" }, 0);
    return l;
}

WCompProcessor::WCompProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                                       .withInput ("Side Chain", AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "WComp", layout())
{
}

bool WCompProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out || (in != AudioChannelSet::mono() && in != AudioChannelSet::stereo())) return false;
    const auto sc = l.inputBuses.size() > 1 ? l.inputBuses[1] : AudioChannelSet::disabled();
    return sc.isDisabled() || sc == AudioChannelSet::mono() || sc == AudioChannelSet::stereo();
}

// the release time of a note value at a tempo; as the reference, a note longer than about 3.4 s (2 bars: 3.96 s) falls
// back to the next shorter straight or dotted note
float wcomp::syncedReleaseMs (int note, double tempo)
{
    static constexpr double beats[] = { 1.0 / 24, 1.0 / 16, 3.0 / 32, 1.0 / 12, 1.0 / 8, 3.0 / 16, 1.0 / 6, 1.0 / 4, 3.0 / 8,
                                        1.0 / 3, 1.0 / 2, 3.0 / 4, 2.0 / 3, 1.0, 1.5, 4.0 / 3, 2.0, 3.0, 4.0, 8.0 };
    tempo = tempo > 0.0 ? tempo : 120.0;
    note = jlimit (0, 19, note);
    for (;;)
    {
        const double ms = beats[note] * 60000.0 / tempo;
        if (note <= 2 || ms <= (note == 19 ? 3960.0 : 3400.0)) return (float) ms;
        note = note == 19 ? 18 : note == 18 ? 17 : note - (note % 3 == 1 ? 2 : 1);   // 2 bars, 1 bar, then x/D, x, (x/2)D ...
    }
}

void WCompProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    prepared = false;
    configure();
}

// Stereo bus + Mode "Stereo": one linked compressor. Stereo bus + Mode "Mono": two independent mono ones. Mono bus: one.
void WCompProcessor::configure()
{
    const int ch = getMainBusNumInputChannels();
    const bool dual = ch > 1 && get (wcomp::mode) > 0.5f;
    layoutDual = dual;
    comps[0].prepare (sr, dual ? 1 : ch);
    comps[1].prepare (sr, 1);
    analogStage.prepare (sr, ch);
    setLatencySamples (comps[0].latency());
    prepared = true;
}

wcomp::Compressor::Settings WCompProcessor::settings() const
{
    wcomp::Compressor::Settings s;
    s.thresholdDb = get (wcomp::threshold);
    s.ratio = get (wcomp::ratio);
    s.attackMs = get (wcomp::attack);
    s.punch = get (wcomp::punch);
    s.mix = get (wcomp::mix) * 0.01f;
    s.outputDb = get (wcomp::output);
    s.limiter = get (wcomp::limiter) > 0.5f;
    const int sync = roundToInt (get (wcomp::sync));
    if (sync == 2) s.releaseMs = get (wcomp::release);
    else
    {
        const double host = hostBpm.load (std::memory_order_relaxed);
        s.releaseMs = wcomp::syncedReleaseMs (roundToInt (get (wcomp::releaseNote)), sync == 0 ? (double) get (wcomp::bpm) : host);
    }
    return s;
}

void WCompProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            hostBpm.store (pos->getBpm() ? *pos->getBpm() : 0.0);
    const int n = b.getNumSamples(), ch = jmin (2, getMainBusNumInputChannels());
    const bool wantDual = ch > 1 && get (wcomp::mode) > 0.5f;
    if (! prepared || wantDual != layoutDual) configure();
    // a side-chain bus, if the host enabled it, is not part of the output (and the reference ignores it)
    for (int c = getMainBusNumOutputChannels(); c < b.getNumChannels(); ++c) b.clear (c, 0, n);

    auto toDb = [] (float v) { return v > 1.0e-5f ? Decibels::gainToDecibels (v) : -100.0f; };
    inLDb.push (toDb (b.getMagnitude (0, 0, n))); inRDb.push (toDb (b.getMagnitude (ch > 1 ? 1 : 0, 0, n)));

    const auto s = settings();
    float* const* w = b.getArrayOfWritePointers();
    if (! layoutDual)
    {
        comps[0].setSettings (s);
        comps[0].process (w, n);
        grDb.push ((float) comps[0].blockGainReductionDb());
        grGain.push (comps[0].blockMeanGain());
        limDb.push ((float) comps[0].blockLimiterDb());
    }
    else
    {
        for (int c = 0; c < 2; ++c)
        {
            comps[c].setSettings (s);
            float* one[1] = { w[c] };
            comps[c].process (one, n);
        }
        grDb.push ((float) jmax (comps[0].blockGainReductionDb(), comps[1].blockGainReductionDb()));
        grGain.push (jmin (comps[0].blockMeanGain(), comps[1].blockMeanGain()));
        limDb.push ((float) jmax (comps[0].blockLimiterDb(), comps[1].blockLimiterDb()));
    }
    // the Analog stage is the last one, after the output gain and the limiter (measured)
    analogStage.setMode (roundToInt (get (wcomp::analog)));
    analogStage.process (w, n);
    outLDb.push (toDb (b.getMagnitude (0, 0, n))); outRDb.push (toDb (b.getMagnitude (ch > 1 ? 1 : 0, 0, n)));
    blocks.fetch_add (1, std::memory_order_relaxed);
}

AudioProcessorEditor* WCompProcessor::createEditor() { return new WCompEditor (*this); }

void WCompProcessor::getStateInformation (MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WCompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WCompProcessor(); }
