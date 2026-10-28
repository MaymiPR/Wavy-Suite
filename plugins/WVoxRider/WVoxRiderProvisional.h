#pragma once
#include <algorithm>
#include <cmath>

// WVox Rider's provisional riding (Phase 2): enough for the editor to move as the reference's does on voice-like
// input, until Phase 3 / 4 identify the real stages. A vocal level (an rms follower of the input, attack by Attack,
// release 1009 ms as the reference's hidden VocRel), a vocal activity gate on a short level (Vocal Sensitivity lowers
// it; Live: never under Spill), a law fitted on two steady buzzes at the defaults (RESEARCH.md, WVox Rider: -27 dBFS
// rms -> +0.69 dB, -21 -> -3.48 dB) clamped to the range and smoothed, and the glide to the Idle value when no vocal is
// detected (after a hold of 500 / 50 ms, 1205 ms as the hidden ParkAtt).
namespace wvoxrider
{
struct RideSettings
{
    double target = -21.0, rangeMin = -6.0, rangeMax = 6.0, idleDb = 0.0, vocalSensitivity = 0.0, spill = -48.0;
    bool fast = false, live = false;
};

class ProvisionalRide
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
    }
    void reset()
    {
        level = shortLevel = 1.0e-10;
        ride = 0.0;
        quiet = 0;
        active = false;
    }

    // one input sample (the mid); returns the ride in dB
    double push (double x, const RideSettings& s)
    {
        const double p = x * x;
        const double att = std::exp (-1.0 / ((s.fast ? 0.010 : 0.050) * fs)), rel = std::exp (-1.0 / (1.009 * fs));
        level = p > level ? p + att * (level - p) : p + rel * (level - p);
        const double k10 = std::exp (-1.0 / (0.010 * fs));
        shortLevel = p + k10 * (shortLevel - p);
        const double shortDb = 10.0 * std::log10 (shortLevel + 1.0e-12);
        levelDb = 10.0 * std::log10 (level + 1.0e-12);
        double gate = -45.0 - s.vocalSensitivity;
        if (s.live) gate = std::max (gate, s.spill);
        const bool voice = shortDb > gate;
        quiet = voice ? 0 : quiet + 1;
        active = quiet < (long) ((s.fast ? 0.05 : 0.5) * fs);
        double to, tau;
        if (active)
        {
            to = std::clamp (0.695 * (s.target - levelDb) - 3.48, s.rangeMin, s.rangeMax);
            tau = s.fast ? 0.08 : 0.25;
        }
        else
        {
            to = std::clamp (s.idleDb, s.rangeMin, s.rangeMax);
            tau = 1.205;
        }
        ride = to + std::exp (-1.0 / (tau * fs)) * (ride - to);
        return ride;
    }

    double levelDb = -120.0;                                    // the vocal level (the meter under Target)
    bool active = false;                                        // vocal detected (the Ride / Idle LEDs)

private:
    double fs = 48000.0, level = 1.0e-10, shortLevel = 1.0e-10, ride = 0.0;
    long quiet = 0;
};
}
