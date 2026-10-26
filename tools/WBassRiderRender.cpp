// Offline renderer for WBass Rider (used by the null tests): the plug-in's own processor, not a copy of its DSP.
//   WBassRiderRender in.f32 out.f32 channels sampleRate [key=value ...]
// Files are raw interleaved 32-bit floats. Keys (real units): target sensitivity spill artifacts rangeMax rangeMin output
// (dB or 0 .. 100), response (0 Slow, 1 Fast), idle (dB, its place in the range as the editor's arrow), mode (0 Stereo,
// 1 Mono, 2 Live Stereo, 3 Live Mono), block (samples, default 512). The output is lined up with the input (the
// reported latency removed, as a host's compensation does), the same length.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WBassRiderProcessor.h"

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: WBassRiderRender in.f32 out.f32 channels fs [key=value ...]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    const int ch = std::max (1, std::min (2, std::atoi (argv[3])));
    const double fs = std::atof (argv[4]);
    int block = 512;
    WBassRiderProcessor p;
    auto set = [&p] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    float idleDbValue = 0.0f;
    bool idleGiven = false;
    for (int i = 5; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq);
        const float v = (float) std::atof (a.c_str() + eq + 1);
        if (k == "block") block = std::max (1, (int) v);
        else if (k == "idle") { idleDbValue = v; idleGiven = true; }
        else set (k.c_str(), v);
    }
    if (idleGiven)       // the arrow's place in the range for the dB asked
    {
        const float lo = p.get (wbassrider::rangeMin), hi = p.get (wbassrider::rangeMax);
        set (wbassrider::idle, hi > lo ? (idleDbValue - lo) / (hi - lo) : 0.5f);
    }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::vector<float> in;
    float tmp[4096];
    size_t got;
    while ((got = std::fread (tmp, sizeof (float), 4096, f)) > 0) in.insert (in.end(), tmp, tmp + got);
    std::fclose (f);
    const long long n = (long long) in.size() / ch;
    p.setPlayConfigDetails (ch, ch, fs, block);
    p.prepareToPlay (fs, block);
    const int lat = p.getLatencySamples();
    juce::AudioBuffer<float> b (ch, block);
    juce::MidiBuffer midi;
    std::vector<float> out ((size_t) (n * ch), 0.0f);
    for (long long s = 0; s < n + lat; s += block)
    {
        const int m = (int) std::min<long long> (block, n + lat - s);
        b.setSize (ch, m, false, false, true);
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < m; ++i) b.setSample (c, i, s + i < n ? in[(size_t) ((s + i) * ch + c)] : 0.0f);
        p.processBlock (b, midi);
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < m; ++i)
            {
                const long long t = s + i - lat;
                if (t >= 0 && t < n) out[(size_t) (t * ch + c)] = b.getSample (c, i);
            }
    }
    FILE* o = std::fopen (argv[2], "wb");
    if (! o) { std::fprintf (stderr, "cannot write %s\n", argv[2]); return 1; }
    std::fwrite (out.data(), sizeof (float), out.size(), o);
    std::fclose (o);
    return 0;
}
