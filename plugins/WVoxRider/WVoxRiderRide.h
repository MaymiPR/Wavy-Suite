#pragma once
#include <algorithm>
#include <cmath>
#include "WVoxRiderFit.h"

// WVox Rider's riding (Phase 4): the reference's measured stages (MEASUREMENTS.md "WVox Rider: Phase 3 / 4") with
// its hidden values; nulltest/vrmodel.c is the same model, line for line. Per input sample (the mid of the input) it
// returns the gain in dB; there is no look-ahead (the reference has no latency).
//   weighting  two shelves, each two identical first-order sections with half the gain (low 100 Hz -18 dB, high
//              6.5 kHz -6 dB)
//   follower   Q, the weighted power plus the floor: up at most by the attack's rate, down at most by the release's
//              (the hold's on samples the detector does not pass), both multiplicative per sample with squared
//              coefficients; it starts at the floor
//   detector   per sample, a short-term power over -(Vocal Sensitivity) - 14 dB times Q, plus the Spill level (Live;
//              off at its minimum)
//   ride       v (the gain less half the Min gain) toward Target / sqrt (Q), only on the samples the detector passes:
//              down by the ride attack, up by the ride release, clamped so that 1/2 gMin + v stays in the range; so
//              the rise is as fast as the detector passes samples (a steady tone just after a drop rises slowly)
//   park       once the detector has passed nothing for the hold (Attack: 500 / 50 ms), v toward the Idle value by
//              the park rate (1205 ms per neper, both ways)
namespace wvoxrider
{
struct RideSettings
{
    double target = -21.0, rangeMin = -6.0, rangeMax = 6.0, idleDb = 0.0, vocalSensitivity = 0.0, spill = -48.0;
    bool fast = false, live = false;

    bool operator== (const RideSettings& o) const
    {
        return target == o.target && rangeMin == o.rangeMin && rangeMax == o.rangeMax && idleDb == o.idleDb
               && vocalSensitivity == o.vocalSensitivity && spill == o.spill && fast == o.fast && live == o.live;
    }
};

class Ride
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        auto shelf = [this] (Section& s, double fc, double gdb, bool low)
        {
            const double g = std::sqrt (std::pow (10.0, gdb / 20.0)), K = std::tan (3.14159265358979323846 * fc / fs);
            double b0, b1, a0, a1;
            if (low) { b0 = 1.0 / K + g; b1 = g - 1.0 / K; a0 = 1.0 / K + 1.0 / g; a1 = 1.0 / g - 1.0 / K; }
            else     { b0 = g / K + 1.0; b1 = 1.0 - g / K; a0 = 1.0 / (g * K) + 1.0; a1 = 1.0 - 1.0 / (g * K); }
            s.b0 = b0 / a0; s.b1 = b1 / a0; s.a1 = a1 / a0;
        };
        shelf (weighting[0], 100.0, -18.0 / 2.0, true); shelf (weighting[1], 100.0, -18.0 / 2.0, true);
        shelf (weighting[2], 6500.0, -6.0 / 2.0, false); shelf (weighting[3], 6500.0, -6.0 / 2.0, false);
        floorPower = std::pow (10.0, fit::bias / 10.0);
        valid = false;
        reset();
    }

    void reset()
    {
        for (auto& s : weighting) s.z = 0.0;
        Q = floorPower;
        shortPower = 0.0;
        voice = false;
        quiet = 0;
        started = false;
    }

    // one input sample; returns the gain in dB
    double push (double x, const RideSettings& s)
    {
        if (! valid || ! (s == cur)) derive (s);
        if (! started) { v = std::clamp (fit::initialV, vlo, vhi); started = true; }
        double y = x;
        for (auto& w : weighting) y = w.run (y);
        const double pw = y * y, q = pw + floorPower;
        // the follower
        if (q > Q) Q = std::min (q, Q * fUp);
        else Q = std::max (q, Q * (voice ? fDown : fHold));
        // the detector
        shortPower = pw > shortPower ? pw + vAtt * (shortPower - pw) : pw + vRel * (shortPower - pw);
        voice = shortPower > threshold * Q + spillPower;
        quiet = voice ? 0 : quiet + 1;
        if (voice)
        {
            const double vd = std::clamp (law / std::sqrt (Q), vlo, vhi);
            if (vd < v) v = std::max (vd, v * rDown);
            else v = std::min (vd, v / rUp);
        }
        else if (quiet > parkHold)
            v = v > vIdle ? std::max (vIdle, v * park) : std::min (vIdle, v / park);
        v = std::clamp (v, vlo, vhi);   // (for a range changed while riding)
        return 20.0 * std::log10 (halfMin + v);
    }

    bool detecting() const { return voice; }                   // vocal detected (the Ride / Idle LEDs)
    double levelDb() const { return 10.0 * std::log10 (Q); }   // the vocal level read (with its floor)

private:
    struct Section
    {
        double b0 = 1.0, b1 = 0.0, a1 = 0.0, z = 0.0;
        double run (double x) { const double y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    };

    // what the settings give (on a change only)
    void derive (const RideSettings& s)
    {
        namespace F = fit;
        auto coef = [this] (double ms) { return std::exp (-1.0 / (fs * ms * 0.001)); };
        const double gmin = std::pow (10.0, s.rangeMin / 20.0), gmax = std::pow (10.0, s.rangeMax / 20.0);
        halfMin = 0.5 * gmin; vlo = halfMin; vhi = gmax - halfMin;
        vIdle = std::clamp (std::pow (10.0, s.idleDb / 20.0) - halfMin, vlo, vhi);
        law = std::pow (10.0, F::c / 20.0) * std::pow (10.0, s.target / 20.0);
        threshold = std::pow (10.0, (-s.vocalSensitivity - F::vk) / 10.0);
        spillPower = (s.live && s.spill > -47.95) ? std::pow (10.0, (s.spill + F::spillOffset) / 10.0) : 0.0;
        fUp = std::pow (coef (s.fast ? F::followerAttackFast : F::followerAttackSlow), -F::followerPower);
        fDown = std::pow (coef (F::followerRelease), F::followerPower);
        fHold = std::pow (coef (F::followerHold), F::followerPower);
        vAtt = coef (F::vadAttack); vRel = coef (F::vadRelease);
        rDown = std::pow (coef (s.fast ? F::rideAttackFast : F::rideAttackSlow), F::ridePower);
        rUp = std::pow (coef (F::rideRelease), F::ridePower);
        park = coef (F::parkTime);
        parkHold = (long) std::floor ((s.fast ? F::parkHoldFast : F::parkHoldSlow) * 0.001 * fs + 0.5);
        cur = s; valid = true;
    }

    double fs = 48000.0;
    Section weighting[4];
    RideSettings cur;
    bool valid = false;
    double floorPower = 1.0e-4, halfMin = 0.25, vlo = 0.25, vhi = 1.75, vIdle = 0.75, law = 0.0891, threshold = 0.04,
           spillPower = 0.0;
    double fUp = 1.0, fDown = 1.0, fHold = 1.0, vAtt = 0.0, vRel = 0.0, rDown = 1.0, rUp = 1.0, park = 1.0;
    long parkHold = 0, quiet = 0;
    double Q = 1.0e-4, shortPower = 0.0, v = 1.0;
    bool voice = false, started = false;
};
}
