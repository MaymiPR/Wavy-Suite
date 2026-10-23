#pragma once
// WComp's VU needle and limiter lamp, as filmed on the reference (MEASUREMENTS.md "WComp needle"; the editor's OpenGL
// frames read back in tools/MeterCapture gl=1, fitted by nulltest/fit_gr_needle.py and fit_io_needle.py).
//
//   strip   the needle takes one of 71 positions: frame k = round (70 v), each at a whole number of degrees from
//           vertical (+ right) about its pivot, -58 ... 59 (the filmed table)
//   GR      v = the compressor's linear gain: the deepest since the last frame shows at once, the return is two
//           one-poles in cascade (28.9 and 142.6 ms) on the linear gain
//   In/Out  v = (peak dBFS + 48) / 48, the peak since the last frame. A rise shows at once and starts a hold that decays
//           from that moment (amplitude, 0.327 s); the first drop after a rise shows the level at once unless that
//           hold is higher, every later drop decays toward the level (amplitude, the same 0.327 s) until the next rise
//   lamp    lit at full when the output limiter acts, fading to 0.64 (0.393 s), off 0.833 s after the last trigger
#include <algorithm>
#include <cmath>

namespace wcomp::needle
{
inline int frame (double v) { return (int) std::lround (std::clamp (70.0 * v, 0.0, 70.0)); }
inline float angle (int k)
{
    static const signed char deg[71] = { -58, -57, -55, -53, -52, -50, -48, -47, -45, -43, -42, -40, -38, -37, -35, -33, -32, -30,
                                         -28, -27, -25, -23, -22, -20, -18, -17, -15, -13, -12, -10, -8, -7, -5, -3, -2, 1, 2, 4,
                                         6, 7, 9, 11, 12, 14, 16, 17, 19, 21, 22, 24, 26, 27, 29, 31, 32, 34, 36, 37, 39, 41, 42,
                                         44, 46, 47, 49, 51, 52, 54, 56, 57, 59 };
    return (float) deg[std::clamp (k, 0, 70)];
}
inline double ioValue (double peak) { return (20.0 * std::log10 (std::max (peak, 1.0e-9)) + 48.0) / 48.0; }

struct Gr
{
    double x1 = 1.0, x2 = 1.0;
    void reset() { x1 = x2 = 1.0; }
    // gainMin: the deepest linear gain since the last call, dt in seconds; returns v (the linear gain shown)
    double step (double gainMin, double dt)
    {
        if (gainMin < x2) { x1 = x2 = gainMin; return x2; }
        const double a1 = std::exp (-dt / 0.0289), a2 = std::exp (-dt / 0.1426);
        x1 = gainMin + a1 * (x1 - gainMin);
        x2 = x1 + a2 * (x2 - x1);
        return x2;
    }
};

struct Io
{
    static constexpr double tau = 0.327;
    double shown = 0.0, hold = 0.0, holdAge = 0.0, env = 0.0;   // linear amplitudes; holdAge: seconds since the rise
    bool armed = false;
    void reset() { *this = Io(); }
    // peak: the highest |sample| since the last call (linear), dt in seconds; returns the amplitude shown
    double step (double peak, double dt)
    {
        holdAge += dt;
        const double h = hold * std::exp (-holdAge / tau);
        if (peak > shown) { hold = env = shown = peak; holdAge = 0.0; armed = false; return shown; }
        if (! armed)
        {
            if (peak < shown) armed = true;
            shown = env = std::max (peak, h);
            return shown;
        }
        env = peak + (env - peak) * std::exp (-dt / tau);
        shown = std::max ({ peak, env, h });
        return shown;
    }
};

struct Lamp
{
    double age = 1.0e9;
    void trigger() { age = 0.0; }
    // advances by dt; returns the lit amount (0: off, 1: full)
    double step (double dt)
    {
        age += dt;
        return age < 0.833 ? 0.64 + 0.36 * std::exp (-age / 0.393) : 0.0;
    }
};
}
