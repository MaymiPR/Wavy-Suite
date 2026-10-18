#pragma once
// WVox DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure and constants come from MEASUREMENTS.md and the Phase 4 identification:
//   * a look-ahead of 64 / 128 / 256 samples (44.1-48 / 88.2-96 / 176.4-192 kHz) with a short gain ramp;
//   * the live level is the reference's true-peak reading (|x| and three parallel all-passes) over a short window;
//   * the static law  gain = 0.99 * M / (1 + (M - 1) * level),  M = 10^(-Compression / 20);
//   * that gain is smoothed in the linear gain domain by two cascaded one-pole stages with separate attack and
//     release times (slow attack, slow release); the applied gain is the smaller of the instant and the smoothed
//     gain, so level rises act at once and falls are followed slowly;
//   * a hard-knee 2:1 downward expander (Gate) built the same way, in front of the compressor;
//   * one gain for both channels (linked) or one per channel.
#include <algorithm>
#include <cmath>
#include <vector>

namespace wvox
{
struct Tuning
{
    int domain = 0;                                          // 0: smooth the gain, 1: smooth the level (amplitude), 2: smooth the level (dB)
    float windowMs = 2.615f;                                 // peak window of the live level
    // gain smoothing (ms): stage 1 attack / release, stage 2 attack / release (compressor)
    float ta1 = 0.56767f, tr1 = 5.069f, ta2 = 184.28f, tr2 = 487.89f;
    // same for the gate
    float gta1 = 404.36f, gtr1 = 0.50083f, gta2 = 143.06f, gtr2 = 0.70844f;
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

// two cascaded one-pole smoothers with separate attack (value falling) and release (value rising) times
class Smoother
{
public:
    void prepare (double fs, float ta1, float tr1, float ta2, float tr2)
    {
        auto k = [fs] (float ms) { return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) fs)); };
        a1 = k (ta1); r1 = k (tr1); a2 = k (ta2); r2 = k (tr2);
        reset (1.0f);
    }
    void reset (float v) { s1 = s2 = v; }
    // target: the instant (ratio) gain; returns the smoothed gain
    float process (float target)
    {
        s1 += (target - s1) * (target < s1 ? a1 : r1);
        s2 += (s1 - s2) * (s1 < s2 ? a2 : r2);
        return s2;
    }
private:
    float a1 = 0.01f, r1 = 0.1f, a2 = 0.01f, r2 = 0.001f, s1 = 1.0f, s2 = 1.0f;
};

// the same two stages applied to the level (amplitude or dB): attack when the level rises, release when it falls
class LevelSmoother
{
public:
    void prepare (double fs, float ta1, float tr1, float ta2, float tr2, bool inDb)
    {
        auto k = [fs] (float ms) { return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) fs)); };
        a1 = k (ta1); r1 = k (tr1); a2 = k (ta2); r2 = k (tr2); db = inDb;
        reset();
    }
    void reset() { s1 = s2 = db ? -120.0f : 0.0f; }
    float process (float f)
    {
        const float v = db ? std::max (20.0f * std::log10 (std::max (f, 1.0e-9f)), -120.0f) : f;
        s1 += (v - s1) * (v > s1 ? a1 : r1);
        s2 += (s1 - s2) * (s1 > s2 ? a2 : r2);
        return db ? std::pow (10.0f, s2 / 20.0f) : s2;
    }
private:
    float a1 = 0.01f, r1 = 0.1f, a2 = 0.01f, r2 = 0.001f, s1 = 0.0f, s2 = 0.0f;
    bool db = false;
};

// Processes up to two channels with one shared (linked) gain. For independent channels run one Processor per channel.
// The level reading is the reference's true-peak detector (identified from its response to short sample patterns,
// error 0.0006): the largest of |x| and the outputs of three parallel first-order all-passes
// (a + z^-1) / (1 + a z^-1), a = 0.1607, 0.3569, 0.6308 (fractional delays of 3/4, 1/2 and 1/4 sample).
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
        const int win = std::max (1, (int) std::lround (t.windowMs * 0.001 * fs));
        peakGate.prepare (win);
        peakComp.prepare (win);
        smGate.prepare (fs, t.gta1, t.gtr1, t.gta2, t.gtr2);
        smComp.prepare (fs, t.ta1, t.tr1, t.ta2, t.tr2);
        lvGate.prepare (fs, t.gta1, t.gtr1, t.gta2, t.gtr2, t.domain == 2);
        lvComp.prepare (fs, t.ta1, t.tr1, t.ta2, t.tr2, t.domain == 2);
        for (auto& d : delay) d.assign ((size_t) size, 0.0f);
        gdb.assign ((size_t) size, 0.0f);
        mf.assign ((size_t) size, 0.0f);
        reset();
    }

    int getLatency() const { return latency; }
    // total gain reduction of the last sample in dB (<= 0): compressor reduction below its idle gain plus the gate
    float getReductionDb() const { return reductionDb; }

    void reset()
    {
        peakGate.reset(); peakComp.reset();
        smGate.reset (1.0f); smComp.reset (1.0f); lvGate.reset(); lvComp.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& c : apX) c = 0.0f;
        for (auto& c : apY) for (auto& v : c) v = 0.0f;
        std::fill (gdb.begin(), gdb.end(), 0.0f);
        std::fill (mf.begin(), mf.end(), 0.0f);
        n = 0; sumMf = 0.0f; reductionDb = 0.0f;
    }

    void setParams (const Params& p) { params = p; }

    // in/out may be the same buffers
    void process (const float* const* in, float* const* out, int numSamples)
    {
        const float M = std::pow (10.0f, -params.compDb / 20.0f);
        const float idleGain = tune.fixedGain * M;
        const float idleDb = 20.0f * std::log10 (idleGain);
        const float outGain = std::pow (10.0f, params.gainDb / 20.0f);
        const bool gateOn = params.gateDb > -79.95f;

        for (int i = 0; i < numSamples; ++i)
        {
            const int idx = n & mask;
            float v = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                const float xs = in[c][i];
                float pk = std::fabs (xs);
                for (int k = 0; k < 3; ++k)
                {
                    const float y = kA[k] * xs + apX[c] - kA[k] * apY[c][k];     // (a + z^-1) / (1 + a z^-1)
                    apY[c][k] = y;
                    pk = std::max (pk, std::fabs (y));
                }
                apX[c] = xs;
                v = std::max (v, pk);
                delay[c][(size_t) idx] = xs;
            }

            // gate: hard-knee 2:1 expander (ratio of the instant gain, smoothed like the compressor gain)
            float gateGain = 1.0f;
            const float fA = peakGate.push (v);
            if (gateOn)
            {
                const float L = 20.0f * std::log10 (std::max (fA, 1.0e-9f));
                if (tune.domain == 0)
                {
                    const float tgt = std::pow (10.0f, std::max (-120.0f, std::min (0.0f, L - params.gateDb)) / 20.0f);
                    gateGain = std::min (tgt, smGate.process (tgt));
                }
                else
                {
                    const float Ls = 20.0f * std::log10 (std::max (std::max (fA, lvGate.process (fA)), 1.0e-9f));
                    gateGain = std::pow (10.0f, std::max (-120.0f, std::min (0.0f, Ls - params.gateDb)) / 20.0f);
                }
            }
            else
            {
                smGate.reset (1.0f);
                if (tune.domain != 0) lvGate.process (fA);
            }

            // compressor: the instant gain follows the live level of the gated signal; the applied gain is the smaller
            // of it and its slow-moving copy (ratio to the idle gain)
            const float fB = peakComp.push (v * gateGain);
            float compRatio;
            if (tune.domain == 0)
            {
                const float ratio = 1.0f / (1.0f + (M - 1.0f) * fB);
                compRatio = std::min (ratio, smComp.process (ratio));
            }
            else
                compRatio = 1.0f / (1.0f + (M - 1.0f) * std::max (fB, lvComp.process (fB)));
            const float totalDb = 20.0f * std::log10 (std::max (gateGain, 1.0e-6f)) + idleDb + 20.0f * std::log10 (std::max (compRatio, 1.0e-6f));
            reductionDb = 20.0f * std::log10 (std::max (gateGain * compRatio, 1.0e-6f));
            gdb[(size_t) idx] = totalDb;

            // look-ahead ramp: minimum over the last `ramp` gains, then their running mean
            float mn = gdb[(size_t) idx];
            for (int k = 1; k < ramp; ++k) mn = std::min (mn, gdb[(size_t) ((n - k) & mask)]);
            mf[(size_t) ((n - ramp + 1) & mask)] = mn;
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
    SlidingMax peakGate, peakComp;
    Smoother smGate, smComp;
    LevelSmoother lvGate, lvComp;
    std::vector<float> delay[2], gdb, mf;
    static constexpr float kA[3] = { 0.1607f, 0.3569f, 0.6308f };
    float apX[2] = {}, apY[2][3] = {};
    int channels = 2, latency = 64, ramp = 16, size = 256, mask = 255, n = 0;
    float sumMf = 0.0f, reductionDb = 0.0f;
};
}
