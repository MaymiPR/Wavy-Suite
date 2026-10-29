#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <random>
#include "WVoxChainFixed.h"
#include "WVoxChainEq.h"
#include "WVoxChainEqTables.h"

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
        else if (shown != to) { shown = to; dirty = true; }
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
    std::array<Biquad, 8> sec;
    int n = 0, count = 0;
    double fs = 48000.0, glide = 0.0, shown = 0.0, to = 0.0;
    bool dirty = true;
};

// the reverb: per colour, two stereo zero-latency convolutions (L->L with R->R, and R->L with L->R on the swapped input)
// with the reference's measured responses at the nearest rate; the last colour keeps running while Reverb is clear
class Reverb
{
public:
    void prepare (double fs, int block);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, int colour, float gain);
    void reset() { for (auto& c : conv) for (auto& e : c) if (e) e->reset(); }

private:
    std::array<std::array<std::unique_ptr<juce::dsp::Convolution>, 2>, 3> conv;
    juce::AudioBuffer<float> a, b;
    int running = 0;                                                  // the colour whose engine runs (0: none yet)
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
    Reverb reverb;
    juce::AudioBuffer<float> tap, wet;
    std::minstd_rand noise { 0x5eed };
    double level = 0.0, levelCoef = 0.0, noiseGain = 0.0;
};
}
