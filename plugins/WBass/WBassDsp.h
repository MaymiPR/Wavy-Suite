#pragma once
// WBass DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure from the Phase 4 identification of the reference (MEASUREMENTS.md, "WBass model"):
//   * original path, per channel: HP3(Freq) when Original Bass is out, LP3(Freq) + HP3(Freq) (a 2nd-order all-pass)
//     when it is in; 3rd-order Butterworths, bilinear with pre-warping (latency 0);
//   * generated path, from the mono sum of the channels:
//       l = LP3(Freq/3) x,  b = LP3(Freq) x
//       stage 1, upward compressor on l: CS1 = C0 (1 + E1 / E01)^-p1, E1 = follower(|b|) (attack / release one-pole)
//       n = HP3(Freq) (CS1 l)                                  the fundamental band of the generated bass
//       q = HP3(Freq/3) (n * CS1 b)                            the harmonic product, scaled by Decay (0 removes it)
//       w = HP3(Freq) (-n + k2 Decay q)
//       stage 2, output level control: G2 = (1 + E2 / E02)^-p2, E2 = follower(|w|)
//       generated = G2 w, times Intensity, added to both channels;
//   * Gain: output trim with a short glide, then a hard clip to the 16-bit range [-1, 32767/32768] (no knee, no time
//     constant: measured with loud 1 kHz tones, where nothing is generated).
#include <algorithm>
#include <cmath>
#include <vector>

namespace wbass
{
inline constexpr double pi = 3.14159265358979323846;

struct Tuning
{
    // fitted to the reference renders at Freq 80 (nulltest/wbass_fitdata.py), see MEASUREMENTS.md
    double C0 = 290.52, E01 = 0.015872, p1 = 0.37264, tau1 = 0.17791;  // stage 1 (tau1: release)
    double att1 = 0.17791;                                         // stage 1 attack (s); = tau1 for a plain one-pole
    bool detectorOnL = false;                                      // stage 1 detector: the LP3(Freq) band, or l
    double E02 = 0.13229, p2 = 0.37777, tau2 = 0.0018317;          // stage 2 (tau2: release)
    double att2 = 0.0018317;                                       // stage 2 attack (s)
    double k2 = -0.0042310;                                        // harmonic product gain at Decay 1
    double gainGlideMs = 25.0;                                     // Out Gain smoothing
    double ceiling = 32767.0 / 32768.0;                            // output clip: [-1, 32767/32768], hard (measured)
};

// one biquad section, transposed direct form II, double precision
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    double run (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// bilinear transform with pre-warping of the analog prototype sections of a 3rd-order Butterworth:
// first-order 1/(s+1) or s/(s+1), and second-order with Q = 1
inline void designFirst (Biquad& s, bool high, double fc, double fs)
{
    const double k = std::tan (pi * fc / fs);
    const double norm = 1.0 / (1.0 + k);
    if (high) { s.b0 = norm; s.b1 = -norm; }
    else      { s.b0 = k * norm; s.b1 = k * norm; }
    s.b2 = 0; s.a1 = (k - 1.0) * norm; s.a2 = 0;
}

inline void designSecond (Biquad& s, bool high, double fc, double fs, double q = 1.0)
{
    const double k = std::tan (pi * fc / fs), k2 = k * k;
    const double norm = 1.0 / (1.0 + k / q + k2);
    if (high) { s.b0 = norm; s.b1 = -2.0 * norm; s.b2 = norm; }
    else      { s.b0 = k2 * norm; s.b1 = 2.0 * k2 * norm; s.b2 = k2 * norm; }
    s.a1 = 2.0 * (k2 - 1.0) * norm;
    s.a2 = (1.0 - k / q + k2) * norm;
}

struct Butter3
{
    Biquad first, second;
    void design (bool high, double fc, double fs) { designFirst (first, high, fc, fs); designSecond (second, high, fc, fs); }
    void reset() { first.reset(); second.reset(); }
    double run (double x) { return second.run (first.run (x)); }
};

inline double onePoleCoef (double tauSeconds, double fs) { return tauSeconds > 0 ? std::exp (-1.0 / (tauSeconds * fs)) : 0.0; }

class Processor
{
public:
    Tuning t;

    void prepare (double sampleRate, int channels)
    {
        fs = sampleRate;
        nCh = std::clamp (channels, 1, 2);
        a1 = onePoleCoef (t.tau1, fs); a1a = onePoleCoef (t.att1, fs);
        a2 = onePoleCoef (t.tau2, fs); a2a = onePoleCoef (t.att2, fs);
        glide = onePoleCoef (t.gainGlideMs * 0.001 / 4.6, fs);   // ~99 % of a step in gainGlideMs
        designFor (freq);
        reset();
    }

    void reset()
    {
        for (auto& c : orig) { c.lp.reset(); c.hp.reset(); }
        lpThird.reset(); lpFreq.reset(); hpN.reset(); hpQ.reset(); hpW.reset();
        e1 = e2 = 0.0;
        gainNow = gainTarget;
    }

    // parameters (call from the audio thread before process)
    void setFreq (double hz) { if (hz != freq) { freq = hz; designFor (freq); } }
    void setIntensityDb (double db) { intensity = std::pow (10.0, db / 20.0); }
    void setOriginalBass (bool in) { origIn = in; }
    void setGainDb (double db) { gainTarget = std::pow (10.0, db / 20.0); }
    void setDecay (double d) { decay = d; }

    // meters of the last block (linear peak values)
    float lastOrigPeak = 0, lastHarmPeak = 0;

    // in-place processing of up to two channels
    void process (float* const* ch, int numChannels, int n)
    {
        const int c = std::min (numChannels, nCh);
        float origPeak = 0, harmPeak = 0;
        for (int i = 0; i < n; ++i)
        {
            double xm = 0;
            for (int k = 0; k < c; ++k) xm += ch[k][i];
            xm /= (double) c;

            // generated path
            const double l = lpThird.run (xm);
            const double b = lpFreq.run (xm);
            const double d1 = std::abs (t.detectorOnL ? l : b);
            const double k1 = d1 > e1 ? a1a : a1;
            e1 = k1 * e1 + (1.0 - k1) * d1;
            const double cs1 = t.C0 * std::pow (1.0 + e1 / t.E01, -t.p1);
            const double nn = hpN.run (cs1 * l);
            const double q = hpQ.run (nn * cs1 * b);
            const double w = hpW.run (-nn + t.k2 * decay * q);
            const double d2 = std::abs (w);
            const double k2c = d2 > e2 ? a2a : a2;
            e2 = k2c * e2 + (1.0 - k2c) * d2;
            const double gen = std::pow (1.0 + e2 / t.E02, -t.p2) * w * intensity;
            harmPeak = std::max (harmPeak, (float) std::abs (gen));
            origPeak = std::max (origPeak, (float) std::abs (b));

            gainNow = glide * gainNow + (1.0 - glide) * gainTarget;
            for (int k = 0; k < c; ++k)
            {
                const double x = ch[k][i];
                const double lo = orig[k].lp.run (x), hi = orig[k].hp.run (x);
                double y = (origIn ? lo + hi : hi) + gen;
                y *= gainNow;
                y = std::clamp (y, -1.0, t.ceiling);
                ch[k][i] = (float) y;
            }
        }
        lastOrigPeak = origPeak; lastHarmPeak = harmPeak;
    }

private:
    struct Orig { Butter3 lp, hp; };
    Orig orig[2];
    Butter3 lpThird, lpFreq, hpN, hpQ, hpW;
    double fs = 48000, freq = 80, intensity = 1, decay = 1, gainTarget = 1, gainNow = 1, glide = 0;
    double a1 = 0, a2 = 0, a1a = 0, a2a = 0, e1 = 0, e2 = 0;
    bool origIn = true;
    int nCh = 2;

    void designFor (double f)
    {
        for (auto& c : orig) { c.lp.design (false, f, fs); c.hp.design (true, f, fs); }
        lpThird.design (false, f / 3.0, fs);
        lpFreq.design (false, f, fs);
        hpN.design (true, f, fs);
        hpQ.design (true, f / 3.0, fs);
        hpW.design (true, f, fs);
    }
};
} // namespace wbass
