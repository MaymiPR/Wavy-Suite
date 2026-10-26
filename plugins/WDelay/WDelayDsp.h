#pragma once
// WDelay's delay path, as identified on the reference (MEASUREMENTS.md "WDelay: H-Delay measurements"):
//
//   w = in + fb y[n-1]                 the loop (each pass adds one sample)
//   v = g w,  g = min (1 / (1 + peak), 1 - r (1 - g[n-1]))     the gain stage: the peak of |w| over the lines (linked),
//                                      taken at once, recovering as a one-pole of 10 ms
//   the line's output through HiPass and LoPass (RBJ biquads, Q 0.8 and 0.95, their frequency warped)
//   y = sum_j h(j - o) v[n - b - j]     a Kaiser-windowed sinc (half-length 511/32, beta 4.968), its 31 taps
//                                      round b = ceil (d), read at (floor (32 d) + 1) / 32 = b + o: one phase past the
//                                      delay d (32 phases per sample at 44.1 / 48 kHz; a whole d is read 1/32 late)
//   D' = max (-1, (Tm - D) / Tm)       the glide (the tape law: at most twice the speed) on D = d + 16, each sample's
//   Tm = (T + 16) (1 + 0.5 depth tri)  step taken before its read; the LFO moves the target: a float triangle in +-1
//                                      from 0 rising, held at its ends (Host / BPM sync: rate x tempo / 60)
//
// Ping Pong: the left line feeds the right at gain 1, the right the left at fb^2; (L + R) / sqrt 2 goes into the start
// side's line. Phase L / R invert their side's input to the delay (the dry signal stays).
// LoFi: the interpolator cut off at 6280 Hz. Mix: equal power. Output: a gain. Header-only (the plug-in and the
// offline renderer share it).
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace wdelay::dsp
{
struct Settings
{
    double timeL = 18000.0, timeR = 18000.0;       // target delays in samples
    float feedback = 0.6f;                          // 0 .. 2
    float mix = 1.0f;                               // 0 (dry) .. 1 (wet)
    float outputDb = 0.0f;
    float hipassHz = 0.0f, lopassHz = 0.0f;         // 0: Off
    float depth = 0.0f, rateHz = 0.1f;              // depth 0 .. 1; the LFO's rate (Host / BPM sync: Rate x tempo / 60)
    bool lofi = false, pingPong = false, startRight = false, phaseL = false, phaseR = false, dual = false;
    int mode = 0;                                   // 0 stereo, 1 mono (one line), 2 mono in, stereo out
};

class Kernels
{
public:
    // the interpolator's taps for each read offset o = u / P - 1 (u = 1 .. P + 1: o from -(P - 1) / P to +1 / P) about
    // the tap centre b = ceil (d); cutoffHz 0: Nyquist (24 kHz above 48 kHz). Per doubling of the rate twice the taps
    // and half the phases per sample (measured at 96 kHz: the same filter in time)
    void build (double fs, double cutoffHz)
    {
        factor = fs > 100000.0 ? 4 : fs > 50000.0 ? 2 : 1;
        phases = 32 / factor;
        taps = 16 * factor - 1;
        width = 2 * taps + 1;
        const double hl = (16.0 - 1.0 / 32.0) * (double) factor, beta = 4.968;
        const double fc = cutoffHz > 0.0 ? cutoffHz / fs : 0.5 / (double) factor;
        table.assign ((size_t) (phases + 2) * (size_t) width, 0.0f);
        const double i0b = bessel0 (beta);
        for (int u = 1; u <= phases + 1; ++u)
        {
            const double o = (double) u / (double) phases - 1.0;
            for (int k = -taps; k <= taps; ++k)
            {
                const double x = (double) k - o;
                double h = 0.0;
                if (std::abs (x) <= hl + 1.0e-9)                     // (the window keeps its edge value, 1 / I0 (beta))
                {
                    const double q = x / hl, w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - q * q))) / i0b;
                    const double a = 2.0 * fc * x * 3.14159265358979323846;
                    h = 2.0 * fc * (std::abs (a) < 1.0e-12 ? 1.0 : std::sin (a) / a) * w;
                }
                table[(size_t) u * (size_t) width + (size_t) (k + taps)] = (float) h;
            }
        }
    }
    const float* offset (int u) const { return table.data() + (size_t) u * (size_t) width; }
    int taps = 0, width = 0, phases = 32, factor = 1;
private:
    static double bessel0 (double x)
    {
        double s = 1.0, t = 1.0;
        for (int k = 1; k < 60; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; if (t < 1.0e-17 * s) break; }
        return s;
    }
    std::vector<float> table;
};

class Biquad
{
public:
    void set (bool hp, double f0, double q, double fs)
    {
        on = f0 > 0.0;
        if (! on || (f0 == freq && hp == high && fs == rate)) return;
        freq = f0; high = hp; rate = fs;
        // the reference's frequency is warped: w0 = 2 atan (pi f / fs), i.e. a cutoff of (fs / pi) atan (pi f / fs)
        // (LoPass 4000 at 3912 Hz, 15000 at 11 859 Hz at 48 kHz; measured)
        const double w = 2.0 * std::atan (3.14159265358979323846 * f0 / fs), al = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1.0 + al;
        b0 = (hp ? (1.0 + c) / 2.0 : (1.0 - c) / 2.0) / a0; b1 = (hp ? -(1.0 + c) : 1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    float process (float x)
    {
        if (! on) return x;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0.0; }
private:
    bool on = false, high = false;
    double freq = -1.0, rate = 0.0, b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
};

class Delay
{
public:
    // fitted on the reference (WDelayRender takes them as glide= modscale= to fit them again)
    double glideOffset = 16.0;                       // the glide's pace: (T - d) / (T + 16) per sample
    double modScale = 0.5;                           // the target's swing at Depth 100: +-50 %
    static constexpr double kReleaseMs = 10.0;      // the gain stage's recovery (per-sample 0.997919 at 48 kHz)

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hifi.build (fs, 0.0);
        lofiK.build (fs, 6277.0);
        int size = 1;
        while (size < (int) (fs * 5.6) + 4 * std::max (hifi.taps, lofiK.taps) + 8) size <<= 1;
        for (auto& l : lines) { l.buf.assign ((size_t) size, 0.0f); l.y1 = 0.0f; l.d = -1.0; l.hp.reset(); l.lp.reset(); }
        mask = size - 1;
        pos = 0;
        gain = 1.0;
        lfoValue = 0.0f;
        lfoUp = true;
        rel = std::exp (-1.0 / (kReleaseMs * 0.001 * fs));
    }

    // ch: the output channels (in place; the input is in them: one channel for mono, two otherwise, the second a copy of
    // the first for mono in and stereo out)
    void process (float* const* ch, int numChannels, int n, const Settings& s)
    {
        const int lineCount = s.mode == 1 || numChannels < 2 ? 1 : 2;
        const bool pp = s.pingPong && lineCount == 2;
        const float dryG = (float) std::cos (s.mix * 1.5707963267948966), wetG = (float) std::sin (s.mix * 1.5707963267948966);
        const float outG = std::pow (10.0f, s.outputDb / 20.0f);
        const Kernels& k = s.lofi ? lofiK : hifi;
        // whole samples; above 48 kHz the reference's delay settles (factor - 1) samples short (measured at 96 kHz: an
        // echo at T - 1 + 1/16)
        const double shortBy = (double) (hifi.factor - 1);
        const double tL = std::max (16.0, std::floor (s.timeL) - shortBy), tR = std::max (16.0, std::floor ((s.dual && ! pp) ? s.timeR : s.timeL) - shortBy);
        for (int i = 0; i < lineCount; ++i)
        {
            lines[(size_t) i].hp.set (true, s.hipassHz, 0.8, fs);
            lines[(size_t) i].lp.set (false, s.lopassHz, 0.95, fs);
            if (lines[(size_t) i].d < 0.0) lines[(size_t) i].d = i == 0 ? tL : tR;     // the first block: no glide
        }
        const float fb = s.feedback;
        for (int j = 0; j < n; ++j)
        {
            const float inL = ch[0][j], inR = numChannels > 1 ? ch[1][j] : inL;
            const float sL = s.phaseL ? -inL : inL, sR = s.phaseR ? -inR : inR;
            float w[2];
            if (pp)
            {
                const float mono = (sL + sR) * 0.70710678f;
                w[0] = (s.startRight ? 0.0f : mono) + fb * fb * lines[1].y1;   // the left line
                w[1] = (s.startRight ? mono : 0.0f) + lines[0].y1;              // the right
            }
            else
            {
                w[0] = sL + fb * lines[0].y1;
                w[1] = sR + fb * lines[1].y1;
            }
            float peak = std::abs (w[0]);
            if (lineCount > 1) peak = std::max (peak, std::abs (w[1]));
            gain = std::min (1.0 / (1.0 + (double) peak), 1.0 - rel * (1.0 - gain));
            // the LFO, as the reference makes it: a float triangle between -1 and +1, from 0 rising, a step of 4 rate / fs,
            // held at each end (the overshoot is lost: up to a sample per turn, and the float steps drift at low rates)
            const double tri = (double) lfoValue;
            // (above 48 kHz the step is the factor times larger: the reference's LFO runs twice as fast at 96 kHz)
            const float step = (float) (4.0 * (double) s.rateHz * (double) hifi.factor / fs);
            lfoValue = lfoUp ? lfoValue + step : lfoValue - step;
            if (lfoValue >= 1.0f) { lfoValue = 1.0f; lfoUp = false; }
            else if (lfoValue <= -1.0f) { lfoValue = -1.0f; lfoUp = true; }
            const double modT = 1.0 + modScale * (double) s.depth * tri;
            float y[2] = { 0.0f, 0.0f };
            for (int i = 0; i < lineCount; ++i)
            {
                auto& l = lines[(size_t) i];
                l.buf[(size_t) pos] = (float) (gain * (double) w[i]);
                // the reference keeps the delay 16 samples longer than the one heard (glideOffset): the glide and the LFO
                // act on that one, D' = max (-1, (Tm - D) / Tm), Tm = (T + 16) (1 + 0.5 depth tri), d = D - 16
                const double Tm = ((i == 0 || pp ? tL : tR) + glideOffset) * modT;
                const double D = l.d + glideOffset;
                l.d = D + std::max (-1.0, (Tm - D) / Tm) - glideOffset;
                // the read: the taps round ceil (d), one phase past the delay
                const double fl = std::floor (l.d);
                int centre, u;
                if (l.d == fl) { centre = (int) fl; u = k.phases + 1; }
                else { centre = (int) fl + 1; u = (int) std::floor ((l.d - fl) * (double) k.phases) + 1; }
                const float* h = k.offset (u);
                float acc = 0.0f;
                const int base = pos - centre;
                // 31 taps at 48 kHz; above, 32 per doubling less one, the first one dropped (96 kHz: -30 .. +31 about a whole d)
                for (int t = -k.taps + k.factor - 1; t <= k.taps; ++t) acc += h[t + k.taps] * l.buf[(size_t) ((base - t) & mask)];
                y[i] = l.lp.process (l.hp.process (acc));            // the filters on the line's output (measured
                                                                     // with a moving delay: before the line they differ)
            }
            for (int i = 0; i < lineCount; ++i) lines[(size_t) i].y1 = y[i];
            pos = (pos + 1) & mask;
            const float wetL = y[0], wetR = lineCount > 1 ? y[1] : y[0];
            ch[0][j] = (dryG * inL + wetG * wetL) * outG;
            if (numChannels > 1) ch[1][j] = (dryG * inR + wetG * wetR) * outG;
        }
    }

private:
    struct Line
    {
        std::vector<float> buf;
        float y1 = 0.0f;
        double d = -1.0;                           // the delay now (samples); < 0: not started
        Biquad hp, lp;
    };
    double fs = 48000.0, gain = 1.0, rel = 0.99776;
    float lfoValue = 0.0f;
    bool lfoUp = true;
    Kernels hifi, lofiK;
    std::array<Line, 2> lines;
    int mask = 0, pos = 0;
};
}
