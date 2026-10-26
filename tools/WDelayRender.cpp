// Offline renderer for the WDelay DSP (used by the null tests).
//   WDelayRender in.f32 out.f32 channels sampleRate [key=value ...] [at=<seconds>:key=value ...]
// Files are raw interleaved 32-bit floats. Keys: time time_r (ms) feedback mix (0 .. 100) output (dB) hipass lopass (Hz,
// 0 Off) depth (0 .. 100) rate (Hz) lofi pingpong start (1: right) phase_l phase_r dual mode (0 stereo, 1 mono, 2 mono
// in stereo out) analog (0 Off, 1 .. 4) block. at=: a change at that time of the input (the block split there; before 0:
// in the pre-roll). preroll=<seconds>: silence first, not written, as tools/RefRender gives the reference: its first
// block at the reference's defaults (375 ms, or time0= in ms; the LFO at rate0=: Rate 0.1 at the host's tempo), then
// the settings.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "WDelayDsp.h"
#include "WCompAnalog.h"
#include "WDelayAnalogTables.h"

static bool exactValues = false;                                  // exact=1: values as given (a preset's), not rounded

static void apply (wdelay::dsp::Settings& s, int& analog, int& block, const std::string& a, double fs)
{
    const auto eq = a.find ('=');
    if (eq == std::string::npos) return;
    const std::string k = a.substr (0, eq);
    double v = std::atof (a.c_str() + eq + 1);
    // the reference's resolutions (as the plug-in, WDelayProcessor.cpp: atResolution; the times and the LFO's rate come
    // computed, their parameters rounded by the caller)
    if (k == "exact") { exactValues = v > 0.5; return; }
    if (exactValues) {}
    else if (k == "feedback" || k == "mix" || k == "depth" || k == "hipass" || k == "lopass") v = std::round (v);
    else if (k == "output") v = std::round (v * 10.0) / 10.0;
    if (k == "time") s.timeL = v * fs / 1000.0;                   // (whole samples or not: Settings::wholeTimes)
    else if (k == "time_r") s.timeR = v * fs / 1000.0;
    else if (k == "feedback") s.feedback = (float) (v / 100.0);
    else if (k == "mix") s.mix = (float) (v / 100.0);
    else if (k == "output") s.outputDb = (float) v;
    else if (k == "hipass") s.hipassHz = (float) v;
    else if (k == "lopass") s.lopassHz = (float) v;
    else if (k == "depth") s.depth = (float) (v / 100.0);
    else if (k == "rate") s.rateHz = (float) v;
    else if (k == "lofi") s.lofi = v > 0.5;
    else if (k == "pingpong") s.pingPong = v > 0.5;
    else if (k == "start") s.startRight = v > 0.5;
    else if (k == "phase_l") s.phaseL = v > 0.5;
    else if (k == "phase_r") s.phaseR = v > 0.5;
    else if (k == "dual") s.dual = v > 0.5;
    else if (k == "mode") s.mode = (int) v;
    else if (k == "analog") analog = (int) v;
    else if (k == "block") block = (int) v;
}

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: WDelayRender in.f32 out.f32 channels fs [key=value ...] [at=t:key=value ...]\n"); return 2; }
    const int ch = std::max (1, std::min (2, std::atoi (argv[3])));
    const double fs = std::atof (argv[4]);
    wdelay::dsp::Settings s;
    s.timeL = s.timeR = std::floor (0.375 * fs);
    int analogMode = 0, block = 512;
    double preroll = 0.0;
    std::vector<std::string> settings;
    std::vector<std::pair<long long, std::string>> changes;
    for (int i = 5; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a.rfind ("at=", 0) == 0)
        {
            const auto colon = a.find (':');
            if (colon != std::string::npos) changes.push_back ({ (long long) std::llround (std::atof (a.c_str() + 3) * fs), a.substr (colon + 1) });
        }
        else if (a.rfind ("preroll=", 0) == 0) preroll = std::atof (a.c_str() + 8);
        else settings.push_back (a);
    }
    for (auto& a : settings) if (a.rfind ("block=", 0) == 0) apply (s, analogMode, block, a, fs);
    std::sort (changes.begin(), changes.end(), [] (auto& x, auto& y) { return x.first < y.first; });
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::fprintf (stderr, "cannot read %s\n", argv[1]); return 1; }
    std::vector<float> in;
    float tmp[4096];
    size_t got;
    while ((got = std::fread (tmp, sizeof (float), 4096, f)) > 0) in.insert (in.end(), tmp, tmp + got);
    std::fclose (f);
    const long long n = (long long) (in.size() / (size_t) ch);
    wdelay::dsp::Delay d;
    for (auto& x : settings)                                      // the fitted constants, to fit them again
    {
        if (x.rfind ("modscale=", 0) == 0) d.modScale = std::atof (x.c_str() + 9);
    }
    d.prepare (fs);
    for (auto& x : settings)
    {
        if (x.rfind ("glide=", 0) == 0) d.glideOffset = std::atof (x.c_str() + 6);
        if (x.rfind ("time0=", 0) == 0) s.timeL = s.timeR = std::floor (std::atof (x.c_str() + 6) * fs / 1000.0);   // the first block's time
        if (x.rfind ("rate0=", 0) == 0) s.rateHz = (float) std::atof (x.c_str() + 6);   // the first block's LFO rate
    }
    wcomp::Analog analog;
    analog.modes = wdelay::tables::analog;                        // H-Delay's modes (4 is H-Comp's)
    analog.prepare (fs, ch);
    std::vector<float> out (in.size());
    std::vector<float> a ((size_t) block), b ((size_t) block);
    size_t next = 0;
    const long long preN = preroll > 0.0 ? std::max<long long> (2LL * block, std::llround (preroll * fs)) : 0;
    for (long long p = 0; p < preN; p += block)                     // the pre-roll: a first block, the settings, the rest
    {
        if (p == block) for (auto& x : settings) apply (s, analogMode, block, x, fs);
        while (p >= block && next < changes.size() && changes[next].first <= p - preN) apply (s, analogMode, block, changes[next++].second, fs);
        std::fill (a.begin(), a.end(), 0.0f); std::fill (b.begin(), b.end(), 0.0f);
        float* chans[2] = { a.data(), b.data() };
        analog.setMode (analogMode);
        analog.process (chans, block);                               // whole blocks, as tools/RefRender renders its pre-roll
        d.process (chans, ch, block, s);
    }
    if (preN == 0) for (auto& x : settings) apply (s, analogMode, block, x, fs);
    for (long long p = 0; p < n; )
    {
        while (next < changes.size() && changes[next].first <= p) apply (s, analogMode, block, changes[next++].second, fs);
        long long len = std::min<long long> ((long long) block, n - p);
        if (next < changes.size()) len = std::min (len, changes[next].first - p);
        if ((long long) a.size() < len) { a.resize ((size_t) len); b.resize ((size_t) len); }
        for (long long j = 0; j < len; ++j)
        {
            a[(size_t) j] = in[(size_t) ((p + j) * ch)];
            b[(size_t) j] = ch > 1 ? in[(size_t) ((p + j) * ch + 1)] : a[(size_t) j];
        }
        float* chans[2] = { a.data(), b.data() };
        analog.setMode (analogMode);                                 // the Analog stage colours the input (measured)
        analog.process (chans, (int) len);
        d.process (chans, ch, (int) len, s);
        for (long long j = 0; j < len; ++j)
        {
            out[(size_t) ((p + j) * ch)] = a[(size_t) j];
            if (ch > 1) out[(size_t) ((p + j) * ch + 1)] = b[(size_t) j];
        }
        p += len;
    }
    FILE* o = std::fopen (argv[2], "wb");
    if (! o) { std::fprintf (stderr, "cannot write %s\n", argv[2]); return 1; }
    std::fwrite (out.data(), sizeof (float), out.size(), o);
    std::fclose (o);
    return 0;
}
