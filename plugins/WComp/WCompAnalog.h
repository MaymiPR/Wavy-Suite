#pragma once
// WComp's Analog stage, as identified on the reference (MEASUREMENTS.md "WComp", nulltest/fit_hc_analog2.py): the last
// stage, after the output gain and the limiter, per channel and the same for every setting of the compressor.
//
//   y = (1 + gain) L(x) + H2( g( H1(x) ) ) + noise
//
// L, H1, H2 are cascades of first-order sections (1 + s/wz) / (1 + s/wp) (bilinear; in mode 3 H1 starts with a
// band-pass biquad), g a measured table (its odd part makes the level-dependent dips and the crossover-like distortion, its even part the even harmonics), and the noise
// is white Gaussian noise shaped to the measured spectrum, independent per channel.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "WCompTables.h"

namespace wcomp
{
class Analog
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::max (1, std::min (2, numChannels));
        for (auto& c : chans) c = {};
        for (int c = 0; c < 2; ++c) chans[(size_t) c].rng = (uint64_t) 0x9E3779B97F4A7C15ull * (uint64_t) (c + 1);
        built = -1;
        setMode (mode);
    }

    // the four modes' tables (H-Comp's by default; WDelay gives H-Delay's, whose modes 1-3 differ)
    const tables::AnalogMode* modes = tables::analog;

    // 0: Off (an identity), 1 .. 4
    void setMode (int m)
    {
        m = std::max (0, std::min (tables::analogModes, m));
        if (m == mode && built == m) return;
        mode = m;
        built = m;
        for (auto& c : chans) { for (auto& s : c.st) s = {}; c.bx1 = c.bx2 = c.by1 = c.by2 = 0.0; std::fill (c.noise.begin(), c.noise.end(), 0.0); c.np = 0; }
        if (m == 0) return;
        const auto& t = modes[m - 1];
        int k = 0;
        for (int i = 0; i < tables::analogLin; ++i) coef[(size_t) k++] = section (t.lin[i][0], t.lin[i][1]);
        for (int i = 0; i < tables::analogPre; ++i) coef[(size_t) k++] = section (t.pre[i][0], t.pre[i][1]);
        for (int i = 0; i < tables::analogPost; ++i) coef[(size_t) k++] = section (t.post[i][0], t.post[i][1]);
        hasBp = t.bpHz > 0.0f;
        if (hasBp)
        {
            // (s / (Q w0)) / (s^2 / w0^2 + s / (Q w0) + 1), bilinear as the fit
            const double w0 = 2.0 * 3.14159265358979323846 * t.bpHz, q = t.bpQ, kb = 2.0 * fs;
            const double a2 = 1.0 / (w0 * w0), a1 = 1.0 / (q * w0), b1 = a1;
            const double d0 = a2 * kb * kb + a1 * kb + 1.0;
            bp = { b1 * kb / d0, 0.0, -b1 * kb / d0, (2.0 - 2.0 * a2 * kb * kb) / d0, (a2 * kb * kb - a1 * kb + 1.0) / d0 };
        }
        g0 = table (0.0, t.g);
        buildNoise (t.noiseDb);
    }

    void process (float* const* ch, int n)
    {
        if (mode == 0) return;
        const auto& t = modes[mode - 1];
        constexpr int nl = tables::analogLin, n1 = tables::analogPre, n2 = tables::analogPost;
        for (int c = 0; c < channels; ++c)
        {
            auto& s = chans[(size_t) c];
            float* d = ch[c];
            for (int i = 0; i < n; ++i)
            {
                const double x = d[i];
                double l = x, u = hasBp ? biquad (s, x) : x;
                for (int k = 0; k < nl; ++k) l = run (k, s.st[(size_t) k], l);
                for (int k = 0; k < n1; ++k) u = run (nl + k, s.st[(size_t) (nl + k)], u);
                double v = table (u, t.g) - g0;
                for (int k = 0; k < n2; ++k) v = run (nl + n1 + k, s.st[(size_t) (nl + n1 + k)], v);
                d[i] = (float) ((1.0 + t.gain) * l + v + noiseSample (s));
            }
        }
    }

private:
    struct Coef { double b0 = 1.0, b1 = 0.0, a1 = 0.0; };
    struct State { double x1 = 0.0, y1 = 0.0; };
    struct Biquad { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };
    struct Channel
    {
        std::array<State, tables::analogLin + tables::analogPre + tables::analogPost> st {};
        double bx1 = 0.0, bx2 = 0.0, by1 = 0.0, by2 = 0.0;
        std::vector<double> noise;
        int np = 0;
        uint64_t rng = 1;
        double spare = 0.0;
        bool hasSpare = false;
    };

    Coef section (double fz, double fp) const
    {
        // (1 + s/wz) / (1 + s/wp), bilinear (s = 2 fs (1 - z^-1) / (1 + z^-1)), as the fit
        const double k = 2.0 * fs, wz = 2.0 * 3.14159265358979323846 * fz, wp = 2.0 * 3.14159265358979323846 * fp;
        const double a0 = 1.0 + k / wp;
        return { (1.0 + k / wz) / a0, (1.0 - k / wz) / a0, (1.0 - k / wp) / a0 };
    }

    double biquad (Channel& s, double x) const
    {
        const double y = bp.b0 * x + bp.b1 * s.bx1 + bp.b2 * s.bx2 - bp.a1 * s.by1 - bp.a2 * s.by2;
        s.bx2 = s.bx1; s.bx1 = x; s.by2 = s.by1; s.by1 = y;
        return y;
    }

    double run (int k, State& s, double x) const
    {
        const auto& c = coef[(size_t) k];
        const double y = c.b0 * x + c.b1 * s.x1 - c.a1 * s.y1;
        s.x1 = x; s.y1 = y;
        return y;
    }

    // piecewise linear over the knots, linear beyond the ends (the end slopes)
    static double table (double u, const float* g)
    {
        using namespace tables;
        constexpr int n = analogKnots;
        int i;
        if (u <= analogKnot[0]) i = 0;
        else if (u >= analogKnot[n - 1]) i = n - 2;
        else
        {
            int lo = 0, hi = n - 1;
            while (hi - lo > 1) { const int m = (lo + hi) / 2; (u < analogKnot[m] ? hi : lo) = m; }
            i = lo;
        }
        const double f = (u - analogKnot[i]) / (analogKnot[i + 1] - analogKnot[i]);
        return g[i] + f * (g[i + 1] - g[i]);
    }

    // a linear-phase FIR whose magnitude is sqrt(PSD fs / 2) (unit-variance white noise in: the measured one-sided
    // spectrum out), frequency-sampled at this rate; above the table's last band the level is held, above 24 kHz it fades
    void buildNoise (const float* psdDb)
    {
        using namespace tables;
        const int taps = fs > 100000.0 ? 512 : fs > 50000.0 ? 256 : 128, half = taps / 2;
        std::vector<double> mag ((size_t) half + 1);
        for (int j = 0; j <= half; ++j)
        {
            const double f = std::max ((double) analogNoiseHz[0], j * fs / taps);
            int i = 0;
            while (i < analogNoiseBands - 2 && f > analogNoiseHz[i + 1]) ++i;
            const double a = std::log (analogNoiseHz[i]), b = std::log (analogNoiseHz[i + 1]);
            const double w = std::max (0.0, std::min (1.0, (std::log (f) - a) / (b - a)));
            double db = psdDb[i] + w * (psdDb[i + 1] - psdDb[i]);
            if (f > 24000.0) db -= 60.0 * std::min (1.0, (f - 24000.0) / 4000.0);
            mag[(size_t) j] = std::sqrt (std::pow (10.0, db / 10.0) * fs / 2.0);
        }
        fir.assign ((size_t) taps, 0.0);
        for (int n = 0; n < taps; ++n)
        {
            double v = mag[0];
            for (int j = 1; j < half; ++j) v += 2.0 * mag[(size_t) j] * std::cos (2.0 * 3.14159265358979323846 * j * (n - half) / taps);
            v += mag[(size_t) half] * std::cos (3.14159265358979323846 * (n - half));
            const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (n + 0.5) / taps);
            fir[(size_t) n] = v / taps * win;
        }
        for (auto& c : chans) { c.noise.assign ((size_t) taps, 0.0); c.np = 0; }
    }

    double gauss (Channel& s)
    {
        if (s.hasSpare) { s.hasSpare = false; return s.spare; }
        auto next = [&s]
        {
            s.rng ^= s.rng >> 12; s.rng ^= s.rng << 25; s.rng ^= s.rng >> 27;
            return ((double) ((s.rng * 0x2545F4914F6CDD1Dull) >> 11) + 0.5) * (1.0 / 9007199254740992.0);
        };
        const double r = std::sqrt (-2.0 * std::log (next())), a = 2.0 * 3.14159265358979323846 * next();
        s.spare = r * std::sin (a); s.hasSpare = true;
        return r * std::cos (a);
    }

    double noiseSample (Channel& s)
    {
        const int taps = (int) fir.size();
        s.noise[(size_t) s.np] = gauss (s);
        double v = 0.0;
        int j = s.np;
        for (int k = 0; k < taps; ++k) { v += fir[(size_t) k] * s.noise[(size_t) j]; j = j == 0 ? taps - 1 : j - 1; }
        s.np = (s.np + 1) % taps;
        return v;
    }

    double fs = 48000.0, g0 = 0.0;
    Biquad bp;
    bool hasBp = false;
    int channels = 2, mode = 0, built = -1;
    std::array<Coef, tables::analogLin + tables::analogPre + tables::analogPost> coef {};
    std::array<Channel, 2> chans {};
    std::vector<double> fir;
};
}
