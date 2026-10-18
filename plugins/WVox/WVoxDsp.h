#pragma once
// WVox DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure and constants come from MEASUREMENTS.md and the Phase 4 identification (see "Detector model"):
//   * level reading: the reference's four-phase true-peak detector, |x| and three parallel first-order all-passes
//     (a + z^-1) / (1 + a z^-1), a = 0.1607, 0.3569, 0.6308 (identified from short sample patterns, error 0.0006);
//   * static law: gain = 0.99 * M / (1 + (M - 1) * level), M = 10^(-Compression / 20) (error < 0.001 dB);
//   * fast path (the look-ahead): the instant gain of every sample, held as a minimum over 65 samples and smoothed by
//     two moving averages of 39 and 26 samples in the linear gain domain, 64 samples of delay (identified from the
//     gain around single-sample and block events on a DC carrier, error 0.05 dB); scaled with the latency at higher
//     sample rates;
//   * slow path: the level (dB) through a fast stage (rise / fall) and a slow stage with a small dead band, both
//     identified from DC level steps; the applied gain is the smaller of the fast and the slow gain;
//   * Gate: hard-knee 2:1 downward expander on a level that rises at once and falls toward the live level as a one-pole in linear amplitude
//     with the GateRel time (130 ms by default; identified on a DC carrier); its gain is shaped by the same two moving
//     averages, so it opens just ahead of a rise; the compressor sees the gated signal;
//   * one gain for both channels (linked) or one Processor per channel.
#include <algorithm>
#include <cmath>
#include <vector>

namespace wvox
{
struct Tuning
{
    // fast path, in samples at 44.1 / 48 kHz (scaled by latency / 64 above)
    int hold = 65, box1 = 39, box2 = 26;
    // slow path (ms, dB): stage 1 rise / fall, dead band, stage 2 rise / fall
    float slowWinMs = 16.349f;       // peak window in front of the slow path
    float ta1 = 34.514f, tr1 = 0.75349f, band = 0.048f, ta2 = 67.961f, tr2 = 488.2f;
    // gate level release (ms): the reference's hidden GateRel parameter, 10 ... 250 ms, default 130
    float gateRelMs = 130.0f;
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
    void reset() { head = tail = 0; time = 0; }
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
    int win = 1, cap = 2, head = 0, tail = 0, time = 0;
};

// moving average over the last L samples
class Box
{
public:
    void prepare (int L, float init) { len = std::max (1, L); buf.assign ((size_t) len, init); sum = init * (float) len; pos = 0; }
    void reset (float init) { std::fill (buf.begin(), buf.end(), init); sum = init * (float) len; pos = 0; }
    float push (float v)
    {
        sum += v - buf[(size_t) pos];
        buf[(size_t) pos] = v;
        if (++pos >= len) { pos = 0; sum = 0.0f; for (float b : buf) sum += b; }   // refresh to stop float drift
        return sum / (float) len;
    }
private:
    std::vector<float> buf;
    float sum = 0.0f;
    int len = 1, pos = 0;
};

// slow level path in dB: stage 1 one-pole with rise / fall times, stage 2 follows stage 1 outside a dead band
class SlowLevel
{
public:
    void prepare (double fs, const Tuning& t)
    {
        auto k = [fs] (float ms) { return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) fs)); };
        a1 = k (t.ta1); r1 = k (t.tr1); a2 = k (t.ta2); r2 = k (t.tr2); band = t.band;
        reset();
    }
    void reset() { s1 = -120.0f; s2 = -120.0f; }
    // v: live amplitude; returns the slow amplitude
    float process (float v)
    {
        const float d = std::max (20.0f * std::log10 (std::max (v, 1.0e-6f)), -120.0f);
        s1 += (d - s1) * (d > s1 ? a1 : r1);
        const float up = s1 - band - s2, dn = s1 + band - s2;
        if (up > 0.0f) s2 += up * a2; else if (dn < 0.0f) s2 += dn * r2;
        return std::pow (10.0f, s2 / 20.0f);
    }
private:
    float a1 = 0.01f, r1 = 0.1f, a2 = 0.01f, r2 = 0.001f, band = 0.0f, s1 = -120.0f, s2 = -120.0f;
};

// Processes up to two channels with one shared (linked) gain. For independent channels run one Processor per channel.
class Processor
{
public:
    struct Params { float compDb = 0.0f, gateDb = -80.0f, gainDb = 0.0f; };

    void prepare (double fs, int numChannels, const Tuning& t = {})
    {
        tune = t;
        channels = std::max (1, std::min (numChannels, 2));
        latency = latencyFor (fs);
        const float scale = (float) latency / 64.0f;
        hold.prepare (std::max (1, (int) std::lround (t.hold * scale)));
        box1.prepare (std::max (1, (int) std::lround (t.box1 * scale)), 1.0f);
        box2.prepare (std::max (1, (int) std::lround (t.box2 * scale)), 1.0f);
        gbox1.prepare (std::max (1, (int) std::lround (t.box1 * scale)), 1.0f);
        gbox2.prepare (std::max (1, (int) std::lround (t.box2 * scale)), 1.0f);
        slow.prepare (fs, t);
        slowWin.prepare (std::max (1, (int) std::lround (t.slowWinMs * 0.001 * fs)));
        gateRho = std::exp (-1.0f / (std::max (t.gateRelMs, 0.1f) * 0.001f * (float) fs));
        size = 1;
        while (size < latency + 8) size <<= 1;
        mask = size - 1;
        for (auto& d : delay) d.assign ((size_t) size, 0.0f);
        reset();
    }

    int getLatency() const { return latency; }
    // total gain reduction of the last sample in dB (<= 0): compressor reduction below its idle gain plus the gate
    float getReductionDb() const { return reductionDb; }

    void reset()
    {
        hold.reset(); box1.reset (1.0f); box2.reset (1.0f); gbox1.reset (1.0f); gbox2.reset (1.0f);
        slow.reset(); slowWin.reset();
        gateLevel = 0.0f;
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& c : apX) c = 0.0f;
        for (auto& c : apY) for (auto& v : c) v = 0.0f;
        n = 0; reductionDb = 0.0f;
    }

    void setParams (const Params& p) { params = p; }

    // in/out may be the same buffers
    void process (const float* const* in, float* const* out, int numSamples)
    {
        const float M = std::pow (10.0f, -params.compDb / 20.0f);
        const float idleGain = tune.fixedGain * M;
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
            if (! (v < 1.0e6f)) v = 1.0e6f;                     // also catches NaN / inf

            // gate: 2:1 below the threshold on a level that rises at once and falls with the GateRel time
            gateLevel = v >= gateLevel ? v : v + (gateLevel - v) * gateRho;      // one-pole toward the live level when it falls
            float gateGain = 1.0f;
            if (gateOn)
            {
                const float L = 20.0f * std::log10 (std::max (gateLevel, 1.0e-9f));
                gateGain = std::pow (10.0f, std::max (-120.0f, std::min (0.0f, L - params.gateDb)) / 20.0f);
            }
            const float gateShaped = gbox2.push (gbox1.push (gateGain));

            // compressor: instant ratio to the idle gain from the live level of the gated signal
            const float ratio = 1.0f / (1.0f + (M - 1.0f) * v * gateGain);
            const float slowRatio = 1.0f / (1.0f + (M - 1.0f) * std::max (v * gateGain, slow.process (slowWin.push (v * gateGain))));

            // fast path: 65-sample hold of the minimum, then two moving averages (identified look-ahead shape)
            const float fastRatio = box2.push (box1.push (-hold.push (-ratio)));
            const float g = gateShaped * std::min (fastRatio, slowRatio);
            reductionDb = 20.0f * std::log10 (std::max (g, 1.0e-6f));

            const float lin = g * idleGain * outGain;
            const int j = n - latency;                                // audio index that leaves now
            for (int c = 0; c < channels; ++c) out[c][i] = delay[c][(size_t) (j & mask)] * lin;
            ++n;
        }
    }

private:
    Tuning tune;
    Params params;
    SlidingMax hold;
    Box box1, box2, gbox1, gbox2;
    SlowLevel slow;
    SlidingMax slowWin;
    std::vector<float> delay[2];
    static constexpr float kA[3] = { 0.1607f, 0.3569f, 0.6308f };
    float apX[2] = {}, apY[2][3] = {};
    float gateLevel = 0.0f, gateRho = 0.0f;
    int channels = 2, latency = 64, size = 128, mask = 127, n = 0;
    float reductionDb = 0.0f;
};
}
