#pragma once
// WBass DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure from the Phase 4 identification of the reference (MEASUREMENTS.md, "WBass model"):
//   * original path, per channel: HP3(Freq) when Original Bass is out, LP3(Freq) + HP3(Freq) (a 2nd-order all-pass)
//     when it is in; 3rd-order Butterworths, bilinear with pre-warping (latency 0);
//   * generated path, from the mono sum of the channels:
//       l = LP3(Freq/3) x,  b = LP3(Freq) x
//       level: d = LP2(cA Freq, QA) LP1(cB Freq) x (a resonant band peaking near Freq),
//              E = (F2(F1(|d|^kr)))^(1/kr), F1 and F2 attack / release one-pole followers in cascade (a second-order,
//              asymmetric smoother: the measured gain settles in about 30 ms after a rise, more slowly after a fall)
//       gain:  CS = C0 (1 + E / E0)^-p, an upward compressor (about 300x at low level)
//       n = C0 c^(1-alpha) HP3(Freq) (c^alpha l), c = CS / C0  (most of the gain after the first high-pass)
//       q = HP3(Freq/3) (n * CS b)                            the harmonic product, scaled by Decay (0 removes it)
//       generated = HP3(Freq) (-n + k2 Decay q), times Intensity, then a peak limiter on the generated signal alone
//       (instant attack, 18.6 ms release, ceiling 0.709: the "clip-free" stage; Intensity is an exact gain below it),
//       added to both channels;
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
    // fitted jointly to the reference renders at Freq 40, 80, 160 and 256 (nulltest/fit_multiF.py, MEASUREMENTS.md)
    double C0 = 287.0496988923884, E0 = 0.0015273781643873486, p = 0.5908406602480677;                // gain law
    double cA = 1.0068735445224746, QA = 1.3539974377418647, cB = 10.374984;                // level band (x Freq)
    double kr = 1.0;                                               // rectifier exponent
    double att1 = 0.4837641317754706, rel1 = 0.00020431050592113843;                       // follower 1 (s) at Freq 80
    double att2 = 0.0004719151170932211, rel2 = 0.010447301303243521;                       // follower 2 (s) at Freq 80
    double s1 = 0.3432486125351782, s2 = 0.1882709219210759;                           // follower times scale with (80 / Freq)^s
    double alpha = 0.046981939933763116;                                             // share of the gain ahead of the first high-pass
    double k2 = -0.007058531827821184;                                                // harmonic product gain at Decay 1
    double limT = 0.708922, limRel = 0.018581;                     // generated-signal limiter: ceiling, release (s)
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
        aLim = onePoleCoef (t.limRel, fs);
        glide = onePoleCoef (t.gainGlideMs * 0.001 / 4.6, fs);   // ~99 % of a step in gainGlideMs
        designFor (freq);
        reset();
    }

    void reset()
    {
        for (auto& c : orig) { c.lp.reset(); c.hp.reset(); }
        lpThird.reset(); lpFreq.reset(); hpN.reset(); hpQ.reset(); hpW.reset(); detLp1.reset(); detLp2.reset();
        e1 = e2 = eLim = 0.0;
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
            const double d = detLp2.run (detLp1.run (xm));
            const double r = t.kr == 1.0 ? std::abs (d) : std::pow (std::abs (d), t.kr);
            const double k1 = r > e1 ? a1a : a1;
            e1 = k1 * e1 + (1.0 - k1) * r;
            const double k2c = e1 > e2 ? a2a : a2;
            e2 = k2c * e2 + (1.0 - k2c) * e1;
            const double level = t.kr == 1.0 ? e2 : std::pow (e2, 1.0 / t.kr);
            const double g = std::pow (1.0 + level / t.E0, -t.p);              // CS / C0
            const double cs = t.C0 * g;
            const double nn = t.C0 * std::pow (g, 1.0 - t.alpha) * hpN.run (std::pow (g, t.alpha) * l);
            const double q = hpQ.run (nn * cs * b);
            double gen = hpW.run (-nn + t.k2 * decay * q) * intensity;
            const double ag = std::abs (gen);
            eLim = ag > eLim ? ag : aLim * eLim + (1.0 - aLim) * ag;
            if (eLim > t.limT) gen *= t.limT / eLim;
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
    Biquad detLp1, detLp2;
    double fs = 48000, freq = 80, intensity = 1, decay = 1, gainTarget = 1, gainNow = 1, glide = 0;
    double a1 = 0, a2 = 0, a1a = 0, a2a = 0, e1 = 0, e2 = 0, aLim = 0, eLim = 0;
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
        const double sc1 = std::pow (80.0 / f, t.s1), sc2 = std::pow (80.0 / f, t.s2);
        a1 = onePoleCoef (t.rel1 * sc1, fs); a1a = onePoleCoef (t.att1 * sc1, fs);
        a2 = onePoleCoef (t.rel2 * sc2, fs); a2a = onePoleCoef (t.att2 * sc2, fs);
        designFirst (detLp1, false, f * t.cB, fs);
        designSecond (detLp2, false, f * t.cA, fs, t.QA);
    }
};
} // namespace wbass
