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
//     averages, so it opens just ahead of a rise; gate and compressor detect the input in parallel and their gains
//     multiply (checked on steady tones to 0.01 dB);
//   * slow path, learned part: a small GRU (SlowNet, trained on renders of the reference by nulltest/train_wvox.py)
//     corrects the analytical slow ratio on a 12 kHz clock; the result never goes below the static-law ratio at the
//     loudest level of the last second, and the correction fades out while the fast ratio stays flat (a steady tone),
//     so steady signals keep the identified paths exactly;
//   * one gain for both channels (linked) or one Processor per channel.
#include <algorithm>
#include <cmath>
#include <vector>
#include "WVoxNet.h"

namespace wvox
{
struct Tuning
{
    // fast path, in samples at 44.1 / 48 kHz (scaled by latency / 64 above)
    int hold = 65, box1 = 39, box2 = 26;
    // slow path (ms, dB): stage 1 rise / fall, dead band, stage 2 rise / fall
    int slowMode = 2;               // 0: two dB stages with dead band; 1: amplitude charge stage + dB/s peak decay;
                                    // 2: amplitude stage (ta1 / tr1), then the gain ratio through a one-pole (ta2 / tr2)
    float decayDbS = 20.0f;         // slowMode 1: fall rate of the held level (dB per second)
    float slowWinMs = 2.1754f;       // peak window in front of the slow path
    float ta1 = 1.5704f, tr1 = 0.67043f, band = 0.048f, ta2 = 249.5f, tr2 = 498.49f;
    // gate level release (ms): the reference's hidden GateRel parameter, 10 ... 250 ms, default 130
    float gateRelMs = 130.0f;
    float fixedGain = 0.99f;        // the reference's constant -0.0873 dB
    bool learned = true;            // the learned correction of the slow path (SlowNet); off = the analytical path only
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

// The learned correction of the slow ratio (nulltest/train_wvox.py, BASE=analytical): one GRU step per 1/12000 s
// (4 samples at 48 kHz; a phase accumulator at other rates). Per step it reads the peak level v, the smallest fast
// ratio and the last analytical slow ratio, plus running minima / maxima of them over the step history; its output c
// (ln) is applied to the following samples, moving linearly from the second-to-last to the last finished step.
class SlowNet
{
public:
    static constexpr int holds[5] = { 12, 30, 60, 120, 240 };     // steps: running minimum of the fast ratio
    static constexpr int peaks[4] = { 120, 360, 1200, 3600 };     // steps: running maximum of the level
    static constexpr int floorSteps = 12000;                      // steps (1 s): the floor's peak hold
    static constexpr int flatSteps = 240;                         // steps (20 ms): steady guard window and fade time
    static constexpr float flatLn = 0.001f;                       // "flat": ln F varied less than this in the window

    void prepare (double fs)
    {
        inc = (float) (12000.0 / fs);
        for (int i = 0; i < 5; ++i) minF[i].prepare (holds[i]);
        for (int i = 0; i < 4; ++i) maxV[i].prepare (peaks[i]);
        floorMax.prepare (floorSteps);
        maxF.prepare (flatSteps);
        reset();
    }
    void reset()
    {
        for (auto& m : minF) m.reset();
        for (auto& m : maxV) m.reset();
        floorMax.reset(); maxF.reset();
        std::fill (h, h + slownet::hidden, 0.0f);
        c1 = c2 = 0.0f; floorLn = 0.0f; phase = 0.0f; gate = 1.0f; resetStep();
    }
    // per sample: v detector level, fast ratio, analytical slow ratio; M = compression factor; returns the slow ratio
    float process (float v, float fast, float slowAn, float M)
    {
        vmax = std::max (vmax, v); fmin = std::min (fmin, fast); slast = slowAn;
        phase += inc;
        const float corr = c2 + (c1 - c2) * std::min (phase, 1.0f);
        const float lnS = std::max (std::log (std::max (slowAn, 1.0e-6f)) + corr, floorLn);
        if (phase >= 0.99999f) { step (M); phase -= 1.0f; if (phase < 1.0e-5f) phase = 0.0f; }
        return std::exp (lnS);
    }

private:
    void resetStep() { vmax = 0.0f; fmin = 1.0e9f; slast = 1.0f; }
    void step (float M)
    {
        const float vb = std::max (vmax, 1.0e-5f);
        const float lv = std::log (vb), lf = std::log (std::max (fmin, 1.0e-6f)), ls = std::log (std::max (slast, 1.0e-6f));
        float in[slownet::inputs];
        in[0] = lv / 5.0f; in[1] = lf / 2.0f; in[2] = ls / 2.0f; in[3] = std::log (M) / 4.0f;
        for (int i = 0; i < 5; ++i) in[4 + i] = -minF[i].push (-lf) / 2.0f;
        for (int i = 0; i < 4; ++i) in[9 + i] = maxV[i].push (lv) / 5.0f;
        floorLn = -std::log1p ((M - 1.0f) * floorMax.push (vb));       // used by the samples of the next step
        const bool flat = maxF.push (lf) - in[8] * 2.0f < flatLn;       // in[8]: minimum over the same 240 steps
        gate = flat ? std::max (0.0f, gate - 1.0f / (float) flatSteps) : std::min (1.0f, gate + 1.0f / (float) flatSteps);

        constexpr int H = slownet::hidden, I = slownet::inputs;
        float gi[3 * H], gh[3 * H];
        for (int j = 0; j < 3 * H; ++j)
        {
            float a = slownet::bih[j], c = slownet::bhh[j];
            for (int k = 0; k < I; ++k) a += slownet::Wih[j * I + k] * in[k];
            for (int m = 0; m < H; ++m) c += slownet::Whh[j * H + m] * h[m];
            gi[j] = a; gh[j] = c;
        }
        float o = slownet::bout[0];
        for (int j = 0; j < H; ++j)
        {
            const float r = 1.0f / (1.0f + std::exp (-(gi[j] + gh[j])));
            const float z = 1.0f / (1.0f + std::exp (-(gi[H + j] + gh[H + j])));
            const float n = std::tanh (gi[2 * H + j] + r * gh[2 * H + j]);
            h[j] = (1.0f - z) * n + z * h[j];
        }
        for (int m = 0; m < H; ++m) o += slownet::Wout[m] * h[m];
        c2 = c1; c1 = o * gate;
        resetStep();
    }

    SlidingMax minF[5], maxV[4], floorMax, maxF;
    float h[slownet::hidden] {};
    float gate = 1.0f, c1 = 0.0f, c2 = 0.0f, floorLn = 0.0f, phase = 0.0f, inc = 0.25f, vmax = 0.0f, fmin = 1.0e9f, slast = 1.0f;
};

// slow level path in dB: stage 1 one-pole with rise / fall times, stage 2 follows stage 1 outside a dead band
class SlowLevel
{
public:
    void prepare (double fs, const Tuning& t)
    {
        auto k = [fs] (float ms) { return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) fs)); };
        a1 = k (t.ta1); r1 = k (t.tr1); a2 = k (t.ta2); r2 = k (t.tr2); band = t.band;
        attackLog = t.slowMode == 3;
        useMin = t.slowMode == 4;
        mode = t.slowMode >= 3 ? 2 : t.slowMode; decay = std::pow (10.0f, -t.decayDbS / (20.0f * (float) fs));
        reset();
    }
    void reset() { s1 = -120.0f; s2 = -120.0f; c1 = 0.0f; h = 0.0f; q = 1.0f; }
    // mode 2: v = live amplitude, M = compression factor; returns the slow gain ratio
    float processRatio (float v, float M)
    {
        c1 += (v - c1) * (v > c1 ? a1 : r1);              // level stage (amplitude)
        const float r = 1.0f / (1.0f + (M - 1.0f) * c1);
        if (r < q) q = attackLog ? q * std::pow (r / q, a2) : q + (r - q) * a2;   // attack (ratio falls): linear or in dB
        else       q += (r - q) * r2;                                             // release in the linear ratio
        return useMin ? std::min (q, r) : q;               // mode 4: the level stage also sets the gain directly
    }
    // v: live amplitude; returns the slow amplitude
    float process (float v)
    {
        if (mode == 1)
        {
            c1 += (v - c1) * (v > c1 ? a1 : r1);          // charge stage in amplitude
            h = std::max (c1, h * decay);                  // held level, falling at decayDbS
            return h;
        }
        const float d = std::max (20.0f * std::log10 (std::max (v, 1.0e-6f)), -120.0f);
        s1 += (d - s1) * (d > s1 ? a1 : r1);
        const float up = s1 - band - s2, dn = s1 + band - s2;
        if (up > 0.0f) s2 += up * a2; else if (dn < 0.0f) s2 += dn * r2;
        return std::pow (10.0f, s2 / 20.0f);
    }
private:
    float a1 = 0.01f, r1 = 0.1f, a2 = 0.01f, r2 = 0.001f, band = 0.0f, s1 = -120.0f, s2 = -120.0f, c1 = 0.0f, h = 0.0f, decay = 1.0f, q = 1.0f;
    int mode = 0;
    bool attackLog = false, useMin = false;
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
        net.prepare (fs);
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
    // deepest gain reduction (dB, <= 0) within the last process() call, for the meter
    float getBlockMinReductionDb() const { return blockMinReductionDb; }

    void reset()
    {
        hold.reset(); box1.reset (1.0f); box2.reset (1.0f); gbox1.reset (1.0f); gbox2.reset (1.0f);
        slow.reset(); slowWin.reset(); net.reset();
        gateLevel = 0.0f;
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& c : apX) c = 0.0f;
        for (auto& c : apY) for (auto& v : c) v = 0.0f;
        n = 0; reductionDb = blockMinReductionDb = 0.0f;
    }

    void setParams (const Params& p) { params = p; }
    // offline analysis only (tools/WVoxRender dump=1): per processed sample the detector level, fast ratio, slow ratio
    // and shaped gate gain are appended here
    std::vector<float>* trace = nullptr;

    // in/out may be the same buffers
    void process (const float* const* in, float* const* out, int numSamples)
    {
        const float M = std::pow (10.0f, -params.compDb / 20.0f);
        const float idleGain = tune.fixedGain * M;
        const float outGain = std::pow (10.0f, params.gainDb / 20.0f);
        const bool gateOn = params.gateDb > -79.95f;
        blockMinReductionDb = 0.0f;

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

            // compressor: instant ratio to the idle gain from the live level of the input (detected in parallel with the gate)
            const float ratio = 1.0f / (1.0f + (M - 1.0f) * v);
            const float vw = slowWin.push (v);
            const float slowAn = tune.slowMode >= 2 ? slow.processRatio (vw, M)
                                                    : 1.0f / (1.0f + (M - 1.0f) * std::max (v, slow.process (vw)));

            // fast path: 65-sample hold of the minimum, then two moving averages (identified look-ahead shape)
            const float fastRatio = box2.push (box1.push (-hold.push (-ratio)));
            const float slowRatio = tune.learned ? net.process (v, fastRatio, slowAn, M) : slowAn;
            const float g = gateShaped * std::min (fastRatio, slowRatio);
            if (trace != nullptr) trace->insert (trace->end(), { v, fastRatio, slowAn, gateShaped });
            reductionDb = 20.0f * std::log10 (std::max (g, 1.0e-6f));
            blockMinReductionDb = std::min (blockMinReductionDb, reductionDb);

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
    SlowNet net;
    std::vector<float> delay[2];
    static constexpr float kA[3] = { 0.1607f, 0.3569f, 0.6308f };
    float apX[2] = {}, apY[2][3] = {};
    float gateLevel = 0.0f, gateRho = 0.0f;
    int channels = 2, latency = 64, size = 128, mask = 127, n = 0;
    float reductionDb = 0.0f, blockMinReductionDb = 0.0f;
};
}
