// Runs WComp's compressor (plugins/WComp/WCompDsp.h) over a mono float32 48 kHz file, both channels the same, in
// 1 ms blocks, and writes per block the deepest gain (the GR meter's input) as float32, plus
// the output audio (float32, left channel). For comparing WComp's meter with the filmed H-Comp needle.
//   wcomp_trace <in.f32> <out_gain.f32> <out_audio.f32> <threshold dB> <ratio> [attack ms] [release ms] [punch]
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../plugins/WComp/WCompDsp.h"

int main (int argc, char** argv)
{
    if (argc < 6) { std::printf ("usage: wcomp_trace in.f32 out_gain.f32 out_audio.f32 thr ratio [attack release punch]\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) return 1;
    std::vector<float> x;
    float v;
    while (std::fread (&v, sizeof v, 1, f) == 1) x.push_back (v);
    std::fclose (f);
    wcomp::Compressor c;
    wcomp::Compressor::Settings s;
    s.thresholdDb = (float) std::atof (argv[4]); s.ratio = (float) std::atof (argv[5]);
    if (argc > 6) s.attackMs = (float) std::atof (argv[6]);
    if (argc > 7) s.releaseMs = (float) std::atof (argv[7]);
    if (argc > 8) s.punch = (float) std::atof (argv[8]);
    c.prepare (48000.0, 2);
    c.setSettings (s);
    const int B = 48;
    std::vector<float> l (B), r (B), gains, out;
    for (size_t i = 0; i + B <= x.size(); i += B)
    {
        for (int k = 0; k < B; ++k) l[(size_t) k] = r[(size_t) k] = x[i + (size_t) k];
        float* ch[2] = { l.data(), r.data() };
        c.process (ch, B);
        gains.push_back ((float) std::pow (10.0, -c.blockGainReductionDb() / 20.0));
        out.insert (out.end(), l.begin(), l.end());
    }
    f = std::fopen (argv[2], "wb"); std::fwrite (gains.data(), sizeof (float), gains.size(), f); std::fclose (f);
    f = std::fopen (argv[3], "wb"); std::fwrite (out.data(), sizeof (float), out.size(), f); std::fclose (f);
    return 0;
}
