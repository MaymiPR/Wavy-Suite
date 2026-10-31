#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <random>
#include "WVoxChainFixed.h"
#include "WVoxChainEq.h"
#include "WVoxChainEqTables.h"
#include "WVoxChainCompress.h"
#include "WVoxChainDeEss.h"
#include "WDelayDsp.h"
#include "WCompAnalog.h"
#include "WDelayAnalogTables.h"

// WVox Chain's audio, as measured on the reference (MEASUREMENTS.md, WVox Chain: Phase 3 and the full campaign):
//   in -> Sensitivity -> Balance (the far side) -> the latency -> Bass, Treble -> Compress -> the de-esser -> [the sends'
//   tap] -> the fixed filter -> dry;  wet: the tap -> the reverb (true stereo convolution) / the delays / the doubler,
//   each muted by its colour's clear position; dry + wet -> Output.
// The fixed filter (every colour clear: two all-passes and gentle shelves) is linear and sits before any dynamics in
// the reference; here it goes after them, on the dry path only, so the wet stages' measured responses (which include
// it) apply to the tap as they are. Phase 4, first pass: the de-esser and Compress pass the signal (their linear part
// only); the delays and the doubler are silent.
namespace wvoxchain::dsp
{
struct Params
{
    float sensitivity = 0.0f, output = 0.0f, balance = 0.0f;
    float fader[6] {};                                                // Bass, Treble, Compress, Reverb, Delay, Pitch
    int colour[6] { 1, 1, 1, 1, 1, 1 };
    double bpm = 120.0;                                               // the host's tempo (the delays are synced to it)
};

// a second-order section, transposed direct form II, per channel
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    void set (const double c[5]) { b0 = c[0]; b1 = c[1]; b2 = c[2]; a1 = c[3]; a2 = c[4]; }
    double run (double x, int ch)
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }
};

// RBJ cookbook sections, the frequency prewarped by the bilinear transform's own warping (as fitted)
inline void rbj (eq::Kind kind, double f0, double q, double gdb, double fs, double c[5])
{
    const double A = std::pow (10.0, gdb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, fs * 0.49, f0) / fs;
    const double cw = std::cos (w), sw = std::sin (w), al = sw / (2.0 * q);
    double b[3], a[3];
    switch (kind)
    {
        case eq::peak: b[0] = 1 + al * A; b[1] = -2 * cw; b[2] = 1 - al * A; a[0] = 1 + al / A; a[1] = -2 * cw; a[2] = 1 - al / A; break;
        case eq::lowShelf:
        {
            const double s = 2 * std::sqrt (A) * al;
            b[0] = A * ((A + 1) - (A - 1) * cw + s); b[1] = 2 * A * ((A - 1) - (A + 1) * cw); b[2] = A * ((A + 1) - (A - 1) * cw - s);
            a[0] = (A + 1) + (A - 1) * cw + s; a[1] = -2 * ((A - 1) + (A + 1) * cw); a[2] = (A + 1) + (A - 1) * cw - s;
            break;
        }
        case eq::highShelf:
        {
            const double s = 2 * std::sqrt (A) * al;
            b[0] = A * ((A + 1) + (A - 1) * cw + s); b[1] = -2 * A * ((A - 1) + (A + 1) * cw); b[2] = A * ((A + 1) + (A - 1) * cw - s);
            a[0] = (A + 1) - (A - 1) * cw + s; a[1] = 2 * ((A - 1) - (A + 1) * cw); a[2] = (A + 1) - (A - 1) * cw - s;
            break;
        }
        case eq::highPass: default: b[0] = (1 + cw) / 2; b[1] = -(1 + cw); b[2] = (1 + cw) / 2; a[0] = 1 + al; a[1] = -2 * cw; a[2] = 1 - al; break;
    }
    c[0] = b[0] / a[0]; c[1] = b[1] / a[0]; c[2] = b[2] / a[0]; c[3] = a[1] / a[0]; c[4] = a[2] / a[0];
}

// Bass or Treble at the (gliding) fader. At the rates the reference was captured at (44.1 / 48 / 88.2 / 96 kHz) each of
// the fader's 201 positions is the reference's own filter (WVoxChainEqTables.h), the nearest 0.1 position of the gliding
// value; at other rates the fitted shapes (WVoxChainEq.h: each section's gain interpolated between the steps, its Q from
// its gain, its frequency fixed)
class EqStage
{
public:
    void prepare (double sampleRate, bool isBass)
    {
        fs = sampleRate; bassStage = isBass;
        glide = std::exp (-1.0 / (0.054 * fs));
        rate = nullptr;
        for (const auto& r : eqtables::rates) if (std::abs (r.fs - fs) < 1.0) rate = &r;
        for (auto& s : sec) s.reset();
        colourIndex = 0; dirty = true;
    }
    // the Bass and Treble faders glide (90 % in about 124 ms, measured): a one-pole of 54 ms on the fader
    void set (int colourIdx, float target)
    {
        colourIdx = juce::jlimit (0, 3, colourIdx);
        if (colourIdx != colourIndex) { colourIndex = colourIdx; for (auto& s : sec) s.reset(); dirty = true; shown = target; }
        to = target;
    }
    void run (double& l, double& r)
    {
        if (colourIndex == 0) return;
        if (std::abs (shown - to) > 1.0e-4) { shown = to + glide * (shown - to); if (++count >= 16) { count = 0; dirty = true; } }
        else if (! juce::exactlyEqual (shown, to)) { shown = to; dirty = true; }
        if (dirty) update();
        for (int i = 0; i < n; ++i) { l = sec[(size_t) i].run (l, 0); r = sec[(size_t) i].run (r, 1); }
    }

private:
    void update()
    {
        dirty = false;
        if (rate != nullptr)
        {
            const int k = juce::jlimit (0, 200, (int) std::lround ((shown + 10.0) * 10.0));
            const auto& pos = (bassStage ? rate->bass : rate->treble)[colourIndex - 1][k];
            n = eqtables::sections;
            for (int i = 0; i < n; ++i) sec[(size_t) i].set (pos.sos[i]);
            return;
        }
        const auto* colour = (bassStage ? eq::bass : eq::treble)[colourIndex];
        const double x = juce::jlimit (0.0, 20.0, shown + 10.0);
        const int i = juce::jmin (19, (int) x);
        const double t = x - i;
        n = 0;
        for (int k = 0; k < colour->n; ++k)
        {
            const auto& s = colour->s[k];
            if (s.kind == eq::highPass && colour->hpFromZero && shown < -1.0e-6) continue;     // (Upper: from 0 up)
            const double g = s.g[i] + (s.g[i + 1] - s.g[i]) * t, q = juce::jmax (0.05, s.q0 + s.q1 * g + s.q2 * g * g), f = s.f;
            double c[5];
            rbj (s.kind, f, q, g, fs, c);
            sec[(size_t) n++].set (c);
        }
    }
    const eqtables::Rate* rate = nullptr;
    bool bassStage = true;
    int colourIndex = 0;
    std::array<Biquad, (size_t) std::max (8, eqtables::sections)> sec;
    int n = 0, count = 0;
    double fs = 48000.0, glide = 0.0, shown = 0.0, to = 0.0;
    bool dirty = true;
};

// the reverb: per colour, two stereo zero-latency convolutions (L->L with R->R, and R->L with L->R on the swapped input)
// with the reference's measured responses at the nearest rate; the last colour keeps running while Reverb is clear
class Reverb
{
public:
    // skip: samples dropped from the responses' (silent) start, the reverb's de-esser's delay
    void prepare (double fs, int block, int skip);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain);
    void reset() { for (auto& c : conv) for (auto& e : c) if (e) e->reset(); }

private:
    std::array<std::array<std::unique_ptr<juce::dsp::Convolution>, 2>, 3> conv;
    juce::AudioBuffer<float> a, b;
    int running = 0;                                                  // the colour whose engine runs (0: none yet)
};

// Compress, as fitted (nulltest/fit_voxchain_compress.py; comp.c is the same algorithm): an input gain into a
// compressor (stereo-linked; a power or peak detector, the threshold, ratio and knee, attack and a two-stage release),
// a static shaper (x^2, x |x|, x^3) before or after it, a look-ahead limiter holding the output under its ceiling, an
// output gain, then the colour's own linear filter (exact per rate). Its parameters per fader step, interpolated. The
// look-ahead delays the path by la samples (also while Compress is clear, a plain delay), taken out of the input delay.
class Compressor
{
public:
    void prepare (double fs);
    int lookAhead() const { return la; }
    void process (float* l, float* r, int n, int colour, float fader);

private:
    void reset();
    double fs = 48000.0;
    int la = 64, w = 0, active = -1;
    std::vector<float> bl, br;
    std::vector<long long> dq;
    std::vector<double> dqNeed;
    long long qh = 0, qt = 0;
    long long count = 0;
    double gf = 0.0, gs = 0.0, pw = 0.0, lg = 1.0, fbL = 0.0, fbR = 0.0, dhx[2] {}, dhy[2] {};
    std::array<Biquad, 4> linear;
};

// The main de-esser, as fitted (nulltest/fit_voxchain_deess.py; deess.c is the same algorithm): a zero-phase high band H
// (its taps per rate) is turned down, x - (1 - g) H x; g from two stereo-linked detectors on the signal after the fixed
// filter: a high-pass one against a threshold that rises with a low-pass one (thr = T + k low, in amplitude), the
// reduction smoothed (attack, hold, release, a slow stage). The gain is computed lookAhead samples before the audio it
// turns down, so the audio is delayed by the band's half length and the look-ahead (taken out of the input delay).
// It turns down one or two signals with the same gain (the main one: the dry path after the fixed filter, and the tap
// before it that the reverb takes). The reverb's send has a second one, the same but for its threshold (the presets'
// -28 dB against the main one's -20; measured: 8 dB lower on every colour), after the main one.
class DeEsser
{
public:
    void prepare (double fs, double thresholdOffsetDb = 0.0);
    void reset();
    int delay() const { return nt + la; }
    // det: what the detectors see (read before a is written, so it may be a); b may be null
    void process (const float* detL, const float* detR, float* aL, float* aR, float* bL, float* bR, int n);

private:
    double fs = 48000.0;
    int nt = 24, la = 24, len = 1, w = 0, gw = 0;
    const double* h = nullptr;
    std::vector<float> al, ar, bl, br, gq;
    Biquad dh, dl;
    double eh = 0.0, el = 0.0, r = 0.0, rs = 0.0, gl = 1.0;
    int hold = 0, holdN = 0;
    double ha = 0, hr = 0, lla = 0, llr = 0, ga = 0, gr = 0, sa = 0, sr = 0, offset = 0;
};

// the delays, as measured (MEASUREMENTS.md, WVox Chain: Phase 4): the send through H-Delay's Analog 4 and its line
// (WDelay's model of H-Delay, set from each colour's preset: Slap a 1/16 note, no feedback, -12 dB; Eight 1/8, 50 %,
// -9 dB; Quarter 1/4, 30 %, -9 dB; high-pass 150 Hz, low-pass 5 kHz; synced to the host's tempo), then SuperTap's two
// taps, each turning the stereo image by 45 degrees: the first at once, L = g1 (L + R), R = g1 (R - L); the second 0.004
// bar later, L = g2 (L - 0.995 R), R = g2 (0.995 L + R). The line keeps running while Delay is clear.
class Delays
{
public:
    void prepare (double fs, int block);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain, double bpm);

private:
    double fs = 48000.0;
    wcomp::Analog analog;
    wdelay::dsp::Delay line;
    juce::AudioBuffer<float> work;
    std::vector<float> histL, histR;                                  // the line's output, for the second tap
    int hpos = 0, running = 1;
};

// the doubler (Pitch), as measured: Waves Doubler's two voices per colour, the left one on the left input, the right
// on the right. Stereo (MEASUREMENTS.md, the doubler's third pass): each voice a linearly interpolated read of the
// send whose speed swings as a triangle (+-7 cents, 0.2 / 0.3 Hz, from the plug-in's start) with a small steady drift,
// the delay one sample longer from the first whole-sample crossing after each turn down until the same after each turn
// up; then a 200 Hz low shelf (-8.6 dB) and a 5.54 kHz high shelf (+4.5 dB), -10 dB at fader 0. Wide (+-18 cents, the
// shelves) and Spreader (+-15 cents, 0.15 Hz, no shelves): the same read head kept within its start +-256 samples: when
// it leaves, a tap one of 20 steps nearer the middle (round (256 2^(k / 20)) samples, where the input best matches
// itself) fades in as the old one fades out. The LFOs run from the plug-in's start (prepare or reset).
class Doubler
{
public:
    void prepare (double fs, int chainLatency);
    void reset();
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain);

private:
    double clockOffset = 0.0;
    // a measured voice's state: the last delay, its direction, the extra sample and the pending turn; the pitch
    // shifter's tap offset, the next one while they crossfade and how far the crossfade is (-1: none)
    struct TriState
    {
        double last = 0.0;
        bool down = false, armed = false;
        double armFloor = 0.0;
        int extra = 0;
        double offset = 0.0, next = 0.0;
        int fade = -1;
        std::array<double, 20> corr {};                                // the input's autocorrelation at the 20 steps
    };
    double triDelay (int colour, int voice, double n) const;           // the smooth delay (samples) at sample n
    void resetVoices (int colour);
    int voicesFor = -1;
    double fs = 48000.0, t = 0.0;
    std::vector<float> lineL, lineR;
    int pos = 0, running = 1;
    std::array<Biquad, 2> shelves;
    int shelvesFor = -1;
    std::array<TriState, 2> tri;
};

class Chain
{
public:
    void prepare (double fs, int maxBlock);
    void reset();
    // stereo in place (Mono/Stereo: the caller copies the input to both channels first)
    void process (float* l, float* r, int n, const Params&);
    // the Sensitivity LED's level: the input's rms (dB), after Sensitivity, over about 10 ms
    float levelDb() const { return level > 1.0e-14 ? (float) (10.0 * std::log10 (level)) : -144.0f; }
    // the Noise (on by default, as the reference; its own state setting, not a host parameter): off for null tests
    bool noiseOn = true;

private:
    double fs = 48000.0;
    int latency = 193, pos = 0;
    std::vector<float> lineL, lineR;
    std::array<Biquad, 4> fixedFilter;
    EqStage bass, treble;
    Compressor compressor;
    DeEsser deesser, reverbDeesser;
    Reverb reverb;
    Delays delays;
    std::array<Biquad, 3> delayFix;                                    // (the echoes' response against the reference's)
    Doubler doubler;
    juce::AudioBuffer<float> tap, send, wet, wet2, wet3;
    std::minstd_rand noise { 0x5eed };
    double level = 0.0, levelCoef = 0.0, noiseGain = 0.0;
};
}
