// Offline renderer for WVox Chain (used by the null tests): the plug-in's own processor, not a copy of its DSP.
//   WVoxChainRender in.f32 out.f32 channels sampleRate [key=value ...]
// Files are raw interleaved 32-bit floats (channels 1: the Mono/Stereo component, the output still stereo; 2: Stereo).
// Keys (real units, the parameter IDs): sensitivity output balance bass treble compress reverb delay pitch, and
// bassColor ... pitchColor (0 clear, 1 .. 3), noise (0 off, as the reference's state setting), block (samples, default 512). The latency is removed; the output is
// stereo and the same length as the input.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WVoxChainProcessor.h"

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: WVoxChainRender in.f32 out.f32 channels fs [key=value ...]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    const int ch = std::max (1, std::min (2, std::atoi (argv[3])));
    const double fs = std::atof (argv[4]);
    int block = 512;
    WVoxChainProcessor p;
    auto set = [&p] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set (wvoxchain::mode, ch == 1 ? 1.0f : 0.0f);
    for (int i = 5; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq);
        const float v = (float) std::atof (a.c_str() + eq + 1);
        if (k == "block") block = std::max (1, (int) v);
        else if (k == "noise") p.setNoise (v != 0.0f);
        else set (k.c_str(), v);
    }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::vector<float> in;
    float tmp[4096];
    size_t got;
    while ((got = std::fread (tmp, sizeof (float), 4096, f)) > 0) in.insert (in.end(), tmp, tmp + got);
    std::fclose (f);
    const long long n = (long long) in.size() / ch;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (ch == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    p.setBusesLayout (layout);
    p.setPlayConfigDetails (ch, 2, fs, block);
    p.prepareToPlay (fs, block);
    const int lat = p.getLatencySamples();
    juce::AudioBuffer<float> b (2, block);
    juce::MidiBuffer midi;
    std::vector<float> out ((size_t) (n * 2), 0.0f);
    for (long long s = 0; s < n + lat; s += block)
    {
        const int m = (int) std::min<long long> (block, n + lat - s);
        b.setSize (2, m, false, false, true);
        b.clear();
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < m; ++i) b.setSample (c, i, s + i < n ? in[(size_t) ((s + i) * ch + c)] : 0.0f);
        p.processBlock (b, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < m; ++i)
            {
                const long long t = s + i - lat;
                if (t >= 0 && t < n) out[(size_t) (t * 2 + c)] = b.getSample (c, i);
            }
    }
    FILE* o = std::fopen (argv[2], "wb");
    if (! o) { std::fprintf (stderr, "cannot write %s\n", argv[2]); return 1; }
    std::fwrite (out.data(), sizeof (float), out.size(), o);
    std::fclose (o);
    return 0;
}
