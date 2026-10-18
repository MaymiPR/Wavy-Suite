// Offline renderer for the WBass DSP (used by the null test and the fitting scripts).
//   WBassRender in.f32 out.f32 channels sampleRate freq intensityDb gainDb originalBassIn [key=value ...]
// Files are raw interleaved 32-bit floats. Keys: C0 E0 p cA QA cB kr ta1 tr1 ta2 tr2 alpha k2 decay ceiling
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "WBassDsp.h"

int main (int argc, char** argv)
{
    if (argc < 9) { std::fprintf (stderr, "usage: WBassRender in.f32 out.f32 channels fs freq intensityDb gainDb origIn [key=value ...]\n"); return 2; }
    const int ch = std::atoi (argv[3]);
    const double fs = std::atof (argv[4]);
    wbass::Processor proc;
    double decay = 1.0;
    for (int i = 9; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq); const double v = std::atof (a.c_str() + eq + 1);
        auto& t = proc.t;
        if (k == "C0") t.C0 = v; else if (k == "E0") t.E0 = v; else if (k == "p") t.p = v;
        else if (k == "cA") t.cA = v; else if (k == "QA") t.QA = v; else if (k == "cB") t.cB = v; else if (k == "kr") t.kr = v;
        else if (k == "ta1") t.att1 = v; else if (k == "tr1") t.rel1 = v; else if (k == "ta2") t.att2 = v; else if (k == "tr2") t.rel2 = v;
        else if (k == "alpha") t.alpha = v; else if (k == "k2") t.k2 = v; else if (k == "limT") t.limT = v; else if (k == "limRel") t.limRel = v; else if (k == "s1") t.s1 = v; else if (k == "s2") t.s2 = v;
        else if (k == "El0") t.El0 = v; else if (k == "pL") t.pL = v; else if (k == "tLa") t.tLa = v; else if (k == "tLr") t.tLr = v; else if (k == "sL") t.sL = v; else if (k == "sA") t.sA = v;
        else if (k == "ceiling") t.ceiling = v; else if (k == "decay") decay = v;
    }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::fseek (f, 0, SEEK_END); const long bytes = std::ftell (f); std::fseek (f, 0, SEEK_SET);
    const int frames = (int) (bytes / (long) (sizeof (float) * (size_t) ch));
    std::vector<float> inter ((size_t) frames * (size_t) ch);
    if (std::fread (inter.data(), sizeof (float), inter.size(), f) != inter.size()) return 1;
    std::fclose (f);

    proc.setFreq (std::atof (argv[5]));
    proc.setIntensityDb (std::atof (argv[6]));
    proc.setGainDb (std::atof (argv[7]));
    proc.setOriginalBass (std::atoi (argv[8]) != 0);
    proc.setDecay (decay);
    proc.prepare (fs, ch);
    std::vector<std::vector<float>> buf ((size_t) ch, std::vector<float> ((size_t) frames));
    for (int i = 0; i < frames; ++i) for (int c = 0; c < ch; ++c) buf[(size_t) c][(size_t) i] = inter[(size_t) i * (size_t) ch + (size_t) c];
    const int block = 512;
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
