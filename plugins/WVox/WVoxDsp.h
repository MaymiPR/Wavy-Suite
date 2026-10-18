#pragma once
// WVox DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure and constants come from MEASUREMENTS.md:
//   * a look-ahead of 64 / 128 / 256 samples (44.1-48 / 88.2-96 / 176.4-192 kHz) with a short gain ramp;
//   * a level detector: peak of a short window, then a slow state with instant attack and a release whose
//     speed grows with the distance (dB) to the live level;
//   * the static law  gain = 0.99 * M / (1 + (M - 1) * level),  M = 10^(-Compression / 20);
//   * a hard-knee 2:1 downward expander (Gate) driven by the same level;
//   * one gain for both channels (linked) or one per channel.
#include <algorithm>
#include <cmath>
#include <vector>

namespace wvox
{
struct Tuning
{
    // slow state s (dB): rises toward the live level with time constant atkMs, falls with
    //   rate (dB/s) = relA * gap^relQ / (1 + gap / relB),   gap = s - live level (dB)
    float atkMs = 85.41f;
    float relA = 5.1943f, relB = 0.5011f, relQ = 1.9141f;
    float windowMs = 0.7653f;       // peak window of the live level (acts at once)
    float slowWindowMs = 9.9983f;   // peak window of the level the slow state follows
    float rampFraction = 0.25f;     // look-ahead ramp length as a fraction of the latency (16 of 64 samples)
    float fixedGain = 0.99f;        // the reference's constant -0.0873 dB
};

inline int latencyFor (double fs) { return fs < 60000.0 ? 64 : fs < 120000.0 ? 128 : 256; }

// sliding-window maximum in O(1) per sample (monotone queue)
class SlidingMax
{
public:
    void prepare (int windowSamples)
    {
        win = std::max (1, windowSamples);
        cap = 1; while (cap < win + 2) cap <<= 1;
        idx.assign ((size_t) cap, 0); val.assign ((size_t) cap, 0.0f);
        reset();
    }
    void reset() { head = tail = 0; count = 0; time = 0; }
    float push (float v)
    {
        while (tail != head && val[(size_t) ((tail - 1) & (cap - 1))] <= v) --tail;
        idx[(size_t) (tail & (cap - 1))] = time; val[(size_t) (tail & (cap - 1))] = v; ++tail;
        while (idx[(size_t) (head & (cap - 1))] <= time - win) ++head;
        ++time;
        return val[(size_t) (head & (cap - 1))];
    }
private:
    std::vector<int> idx; std::vector<float> val;
    int win = 1, cap = 2, head = 0, tail = 0, count = 0, time = 0;
};

// level detector: live level = peak of a short window (used at once); slow state s follows the peak of a longer
// window (slow attack, level-dependent release); the detected level is the larger of the two.
class Detector
{
public:
    void prepare (double fs, const Tuning& t)
    {
        tune = t;
        fast.prepare ((int) std::lround (t.windowMs * 0.001 * fs));
        slow.prepare ((int) std::lround (t.slowWindowMs * 0.001 * fs));
        sampleRate = (float) fs;
        atk = 1.0f - std::exp (-1.0f / (std::max (t.atkMs, 0.01f) * 0.001f * (float) fs));
        reset();
    }
    void reset() { fast.reset(); slow.reset(); sDb = kFloorDb; }

    // v: rectified live sample (the louder channel when linked). Returns the detected amplitude.
    float process (float v)
    {
        const float f = fast.push (v);
        const float fSlow = slow.push (v);
        const float tDb = std::max (20.0f * std::log10 (std::max (fSlow, 1.0e-9f)), kFloorDb);
        if (tDb > sDb)
            sDb += (tDb - sDb) * atk;
        else
        {
            const float gap = std::min (sDb - tDb, 120.0f);
            const float rate = tune.relA * std::pow (gap, tune.relQ) / (1.0f + gap / tune.relB);   // dB per second
            sDb = std::max (tDb, sDb - rate / sampleRate);
        }
        return std::max (f, std::pow (10.0f, sDb / 20.0f));
    }

private:
    static constexpr float kFloorDb = -100.0f;
    Tuning tune;
    SlidingMax fast, slow;
    float sampleRate = 48000.0f, sDb = kFloorDb, atk = 0.0002f;
};

// Processes up to two channels with one shared (linked) gain. For independent channels run one Processor per channel.
// The level detector reads the true (inter-sample) peak of each channel: 4x interpolation with an 8-tap windowed
// sinc, evaluated 4 samples late (the look-ahead hides that).
class Processor
{
public:
    struct Params { float compDb = 0.0f, gateDb = -80.0f, gainDb = 0.0f; };

    void prepare (double fs, int numChannels, const Tuning& t = {})
    {
        tune = t;
        channels = std::max (1, std::min (numChannels, 2));
        latency = latencyFor (fs);
        ramp = std::max (1, (int) std::lround (latency * t.rampFraction));
        size = 1;
        while (size < latency + 4 * ramp + 16) size <<= 1;
        mask = size - 1;
        gate.prepare (fs, t);
        comp.prepare (fs, t);
        for (auto& d : delay) d.assign ((size_t) size, 0.0f);
        gdb.assign ((size_t) size, 0.0f);
        mf.assign ((size_t) size, 0.0f);
        // interpolation kernel: taps for x[k-3..k+4] at fractional positions 1/4, 1/2, 3/4 between x[k] and x[k+1]
        const double pi = 3.14159265358979323846;
        for (int f = 0; f < 3; ++f)
        {
            const double d = 0.25 * (f + 1);
            for (int i = 0; i < 8; ++i)
            {
                const double xk = (double) (i - 3) - d;                      // distance from the interpolated point
                const double sinc = std::fabs (xk) < 1.0e-9 ? 1.0 : std::sin (pi * xk) / (pi * xk);
                const double w = 0.42 + 0.5 * std::cos (pi * xk / 4.5) + 0.08 * std::cos (2.0 * pi * xk / 4.5);   // Blackman, 9 wide
                kern[f][i] = (float) (sinc * w);
            }
        }
        reset();
    }

    int getLatency() const { return latency; }
    // total gain reduction of the last sample in dB (<= 0): compressor reduction below its idle gain plus the gate
    float getReductionDb() const { return reductionDb; }

    void reset()
    {
        gate.reset(); comp.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& h : hist) std::fill (std::begin (h), std::end (h), 0.0f);
        std::fill (gdb.begin(), gdb.end(), 0.0f);
        std::fill (mf.begin(), mf.end(), 0.0f);
        n = 0; sumMf = 0.0f;
    }

    void setParams (const Params& p) { params = p; }

    // in/out may be the same buffers
    void process (const float* const* in, float* const* out, int numSamples)
    {
        const float M = std::pow (10.0f, -params.compDb / 20.0f);
        const float outGain = std::pow (10.0f, params.gainDb / 20.0f);
        const bool gateOn = params.gateDb > -79.95f;

        for (int i = 0; i < numSamples; ++i)
        {
            const int idx = n & mask;
            float v = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                float* h = hist[c];                                   // h[0..7] = x[n-7..n], so the pair (x[n-4], x[n-3]) sits at h[3], h[4]
                for (int k = 0; k < 7; ++k) h[k] = h[k + 1];
                h[7] = in[c][i];
                float pk = std::fabs (h[3]);
                for (int f = 0; f < 3; ++f)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < 8; ++k) acc += kern[f][k] * h[k];
                    pk = std::max (pk, std::fabs (acc));
                }
                v = std::max (v, pk);
                delay[c][(size_t) idx] = in[c][i];
            }
            const int m = n - 4;                                      // audio index the detector now speaks for

            // gate: hard-knee 2:1 expander on the slow-state level
            float gateDb = 0.0f;
            const float sA = gate.process (v);
            if (gateOn)
            {
                const float L = 20.0f * std::log10 (std::max (sA, 1.0e-9f));
                gateDb = std::max (-120.0f, std::min (0.0f, L - params.gateDb));
            }
            // the compressor sees the gated signal
            const float sB = comp.process (v * std::pow (10.0f, gateDb / 20.0f));
            const float law = tune.fixedGain * M / (1.0f + (M - 1.0f) * sB);
            const float lawDb = 20.0f * std::log10 (law);
            reductionDb = gateDb + std::min (0.0f, lawDb - 20.0f * std::log10 (tune.fixedGain * M));
            gdb[(size_t) (m & mask)] = gateDb + lawDb;

            // look-ahead ramp: minimum over the last `ramp` gains, then their running mean
            float mn = gdb[(size_t) (m & mask)];
            for (int k = 1; k < ramp; ++k) mn = std::min (mn, gdb[(size_t) ((m - k) & mask)]);
            mf[(size_t) ((m - ramp + 1) & mask)] = mn;
            const int j = n - latency;                                // audio index that leaves now
            sumMf += mf[(size_t) (j & mask)] - mf[(size_t) ((j - ramp) & mask)];
            const float lin = std::pow (10.0f, sumMf / (float) ramp / 20.0f) * outGain;
            for (int c = 0; c < channels; ++c) out[c][i] = delay[c][(size_t) (j & mask)] * lin;
            ++n;
        }
    }

private:
    Tuning tune;
    Params params;
    Detector gate, comp;
    std::vector<float> delay[2], gdb, mf;
    float kern[3][8] = {}, hist[2][8] = {};
    int channels = 2, latency = 64, ramp = 16, size = 256, mask = 255, n = 0;
    float sumMf = 0.0f, reductionDb = 0.0f;
};
}
