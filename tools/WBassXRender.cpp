// Offline renderer for the WBassX DSP (used by the null test).
//   WBassXRender in.f32 out.f32 channels sampleRate [key=value ...]
// Files are raw interleaved 32-bit floats. Keys: input freq decay response ratio highpass orig harm monitor block
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "WBassXDsp.h"

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: WBassXRender in.f32 out.f32 channels fs [key=value ...]\n"); return 2; }
    const int ch = std::atoi (argv[3]);
    const double fs = std::atof (argv[4]);
    wbassx::Processor proc;
    int block = 512;
    proc.setHarmonicsDb (-100.0);
    for (int i = 5; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq); const double v = std::atof (a.c_str() + eq + 1);
        if (k == "input") proc.setInputDb (v); else if (k == "freq") proc.setFreq (v); else if (k == "decay") proc.setDecayDb (v);
        else if (k == "response") proc.setResponseMs (v); else if (k == "ratio") proc.setRatio (v); else if (k == "highpass") proc.setHighPass ((int) v);
        else if (k == "orig") proc.setOrigBassDb (v); else if (k == "harm") proc.setHarmonicsDb (v); else if (k == "monitor") proc.setMonitor ((int) v);
        else if (k == "block") block = (int) v;
    }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::fseek (f, 0, SEEK_END); const long bytes = std::ftell (f); std::fseek (f, 0, SEEK_SET);
    const int frames = (int) (bytes / (long) (sizeof (float) * (size_t) ch));
    std::vector<float> inter ((size_t) frames * (size_t) ch);
    if (std::fread (inter.data(), sizeof (float), inter.size(), f) != inter.size()) return 1;
    std::fclose (f);
    proc.prepare (fs, ch);
    std::vector<std::vector<float>> buf ((size_t) ch, std::vector<float> ((size_t) frames));
    for (int i = 0; i < frames; ++i) for (int c = 0; c < ch; ++c) buf[(size_t) c][(size_t) i] = inter[(size_t) i * (size_t) ch + (size_t) c];
    for (int s = 0; s < frames; s += block)
    {
        float* p[2] = { buf[0].data() + s, ch > 1 ? buf[1].data() + s : nullptr };
        proc.process (p, ch, std::min (block, frames - s));
    }
    for (int i = 0; i < frames; ++i) for (int c = 0; c < ch; ++c) inter[(size_t) i * (size_t) ch + (size_t) c] = buf[(size_t) c][(size_t) i];
    f = std::fopen (argv[2], "wb");
    std::fwrite (inter.data(), sizeof (float), inter.size(), f);
    std::fclose (f);
    return 0;
}
