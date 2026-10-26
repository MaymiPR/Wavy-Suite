#pragma once
#include <algorithm>
#include <cmath>

// A PROVISIONAL riding model (Phase 2), so the editor responds to audio as the reference's does until the DSP is
// identified (Phase 3 / 4). Fitted on films of the reference's editor with steady tones and a bass line
// (nulltest/ride_provisional.py, MeterCapture gl=1): the Energy meter within 0.04 dB on steady tones (1.5 dB median on
// a bass line), the Rider within 0.1 dB on steady tones (1.8 dB median on the bass line). Not a null: the reference's
// note detector (Sensitivity, Response, Artifacts) is not modelled; Spill is the gate.
//   w      = first-order high-pass (fc) of the input (the energy weighting)
//   fast   = peak follower of |w| over each millisecond: instant rise, one-pole release (relF)
//   slow   = one-pole of fast, attack attS, release relS
//   energy = 20 log10 (fast) + off                       (what the Energy meter shows)
//   a note begins above the gate when fast rises onDb over slow (or from rest); for its first `settle` the gain
//   follows target - max (slow energy, energy - 12 dB), then it holds; it ends when fast falls offDb under the note's
//   peak or under the gate, and after holdMs the gain glides back to the Idle value (tau)
namespace wbassride
{
struct ProvisionalRide
{
    static constexpr double fc = 179.29, relF = 0.053, attS = 0.0375, relS = 0.9998, off = 2.9227, onDb = 5.6924,
                            settle = 0.0967, offDb = 23.6687, holdMs = 54.99, tau = 0.0388;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        step = std::max (1, (int) std::lround (fs / 1000.0));
        const double dt = step / fs;
        kF = std::exp (-dt / relF); kA = std::exp (-dt / attS); kR = std::exp (-dt / relS); kT = std::exp (-dt / tau);
        const double w = std::tan (3.141592653589793 * fc / fs);       // first-order high-pass (bilinear)
        b0 = 1.0 / (1.0 + w); a1 = (1.0 - w) / (1.0 + w);
        x1[0] = x1[1] = y1[0] = y1[1] = 0.0;
        fast = slow = 1.0e-6; peakNow = 0.0; count = 0; state = Idle; t = 0.0;
    }

    // one input sample (each channel); returns true when the control moved on (once per millisecond)
    bool push (const float* in, int channels, double target, double lo, double hi, double idleDb, double gateDb)
    {
        for (int c = 0; c < channels && c < 2; ++c)
        {
            const double y = b0 * ((double) in[c] - x1[c]) + a1 * y1[c];
            x1[c] = in[c]; y1[c] = y;
            peakNow = std::max (peakNow, std::abs (y));
        }
        if (++count < step) return false;
        count = 0;
        const double dt = step / fs;
        t += dt;
        const double v = peakNow;
        peakNow = 0.0;
        fast = v > fast ? v : fast * kF + v * (1.0 - kF);
        const double k = fast > slow ? kA : kR;
        slow = slow * k + fast * (1.0 - k);
        energy = 20.0 * std::log10 (std::max (fast, 1.0e-6)) + off;
        const double es = 20.0 * std::log10 (std::max (slow, 1.0e-6)) + off;
        const bool above = energy > gateDb;
        if (above && (state != Note || (energy - es > onDb && t - tOn > 0.05)))
        {
            state = Note; tOn = t; notePeak = fast;
        }
        if (state == Note)
        {
            notePeak = std::max (notePeak, fast);
            if (t - tOn < settle) gain = std::clamp (target - std::max (es, energy - 12.0), lo, hi);
            if (! above || 20.0 * std::log10 (std::max (fast, 1.0e-9) / notePeak) < -offDb) { state = Release; tOff = t; }
        }
        if (state != Note && (state == Idle || t - tOff > holdMs * 0.001))
        {
            gain = idleDb + (gain - idleDb) * kT;
            if (state == Release && std::abs (gain - idleDb) < 0.01) state = Idle;
        }
        return true;
    }

    double gain = 0.0, energy = -100.0;

private:
    enum State { Idle, Note, Release };
    double fs = 48000.0, kF = 0.0, kA = 0.0, kR = 0.0, kT = 0.0, b0 = 1.0, a1 = 0.0;
    double x1[2] {}, y1[2] {};
    double fast = 1.0e-6, slow = 1.0e-6, peakNow = 0.0, notePeak = 0.0, t = 0.0, tOn = -1.0, tOff = 0.0;
    int step = 48, count = 0;
    State state = Idle;
};
}
