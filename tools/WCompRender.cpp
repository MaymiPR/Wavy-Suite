// Offline renderer for the WComp DSP (used by the null tests).
//   WCompRender in.f32 out.f32 channels sampleRate [key=value ...]
// Files are raw interleaved 32-bit floats; the output is aligned with the input (the latency removed).
// Keys: threshold ratio attack release punch mix (0..100) output limiter (0/1) analog (0 Off, 1..4) block
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "WCompDsp.h"
#include "WCompAnalog.h"

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: WCompRender in.f32 out.f32 channels fs [key=value ...]\n"); return 2; }
    const int ch = std::atoi (argv[3]);
    const double fs = std::atof (argv[4]);
    wcomp::Compressor comp;
    wcomp::Compressor::Settings s;
    wcomp::Analog analog;
    int block = 512, analogMode = 0;
    for (int i = 5; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq); const float v = (float) std::atof (a.c_str() + eq + 1);
        if (k == "threshold") s.thresholdDb = v; else if (k == "ratio") s.ratio = v; else if (k == "attack") s.attackMs = v;
        else if (k == "release") s.releaseMs = v; else if (k == "punch") s.punch = v; else if (k == "mix") s.mix = v / 100.0f;
        else if (k == "output") s.outputDb = v; else if (k == "limiter") s.limiter = v > 0.5f; else if (k == "block") block = (int) v;
        else if (k == "analog") analogMode = (int) v;
        else if (k == "limest") comp.limEstimate = (int) v;
    }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::fseek (f, 0, SEEK_END); const long bytes = std::ftell (f); std::fseek (f, 0, SEEK_SET);
    const int frames = (int) (bytes / (long) (sizeof (float) * (size_t) ch));
    std::vector<float> inter ((size_t) frames * (size_t) ch);
    if (std::fread (inter.data(), sizeof (float), inter.size(), f) != inter.size()) return 1;
    std::fclose (f);
    comp.prepare (fs, ch);
    comp.setSettings (s);
    analog.prepare (fs, ch);
    analog.setMode (analogMode);
    const int lat = comp.latency();
    const int total = frames + lat;
    std::vector<std::vector<float>> buf ((size_t) ch, std::vector<float> ((size_t) total, 0.0f));
    for (int i = 0; i < frames; ++i) for (int c = 0; c < ch; ++c) buf[(size_t) c][(size_t) i] = inter[(size_t) i * (size_t) ch + (size_t) c];
    for (int p = 0; p < total; p += block)
    {
        float* q[2] = { buf[0].data() + p, ch > 1 ? buf[1].data() + p : nullptr };
        comp.process (q, std::min (block, total - p));
        analog.process (q, std::min (block, total - p));
    }
    for (int i = 0; i < frames; ++i) for (int c = 0; c < ch; ++c) inter[(size_t) i * (size_t) ch + (size_t) c] = buf[(size_t) c][(size_t) (i + lat)];
    f = std::fopen (argv[2], "wb");
    std::fwrite (inter.data(), sizeof (float), inter.size(), f);
    std::fclose (f);
    return 0;
}
