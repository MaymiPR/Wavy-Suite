#pragma once
// WBassX DSP: dependency-free (std only) so it runs in the plug-in, the unit tests and the offline render tool.
// Structure from the Phase 4 identification of the reference (MEASUREMENTS.md, "WBassX model"), per channel:
//   * Input: a plain trim ahead of everything (stops at 0 dB).
//   * original path: HP3(F) x + OrigBass * LP3(F) x, 3rd-order Butterworths, bilinear with pre-warping (latency 0);
//     at Original Bass 0 dB that is a flat all-pass.
//   * generated part G:
//       u = LP3(F) x                                   (the same low-pass as the original path)
//       w = g u                                        g <= 1, the level control (Ratio, Response): learned (a GRU);
//                                                      it listens after the high-pass (it depends on the High-pass
//                                                      setting), so it also reads z = K(F) HP_G(u)
//       y[n] = w[n] (1 + 8 D v[n-1]),  v = HP2_B(y)    the Decay feedback (D = 10^(Decay/20)) with one sample of
//                                                      delay, y held within +-1; HP2_B: Butterworth at F/3, held
//                                                      within 20 ... 40 Hz. Like the reference it oscillates at
//                                                      Nyquist when 8 D b0 |w| > 1 (extreme Decay / Ratio settings)
//       G = -K(F) HP_G(y)                              K(F): measured per whole-Hz Freq setting;
//                                                      HP_G: 6 dB/oct  HP2(16 Hz)^2, 12 dB/oct  HP2(F) HP2(16 Hz),
//                                                            24 dB/oct  HP2(0.8 F)^2  (Butterworth sections)
//   * Monitor: Audio = original path + Harmonics * G (Harmonics capped at +10.96 dB); Harmonics plays G and Original
//     Bass plays LP3(F) x, both pre-fader.
//   * output: a hard clip at +-1.
// No parameter smoothing (the reference changes at block boundaries).
#include <algorithm>
#include <cmath>
#include "WBassXNet.h"
#if defined (__SSE2__) || defined (_M_X64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <emmintrin.h>
 #define WBASSX_SIMD_SSE 1
#elif defined (__ARM_NEON) || defined (__ARM_NEON__) || defined (_M_ARM64)
 #include <arm_neon.h>
 #define WBASSX_SIMD_NEON 1
#endif

namespace wbassx
{
inline constexpr double pi = 3.14159265358979323846;

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    double run (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline void designFirst (Biquad& s, bool high, double fc, double fs)
{
    const double k = std::tan (pi * fc / fs), norm = 1.0 / (1.0 + k);
    if (high) { s.b0 = norm; s.b1 = -norm; } else { s.b0 = k * norm; s.b1 = k * norm; }
    s.b2 = 0; s.a1 = (k - 1.0) * norm; s.a2 = 0;
}

inline void designSecond (Biquad& s, bool high, double fc, double fs, double q)
{
    const double k = std::tan (pi * fc / fs), k2 = k * k, norm = 1.0 / (1.0 + k / q + k2);
    if (high) { s.b0 = norm; s.b1 = -2.0 * norm; s.b2 = norm; } else { s.b0 = k2 * norm; s.b1 = 2.0 * k2 * norm; s.b2 = k2 * norm; }
    s.a1 = 2.0 * (k2 - 1.0) * norm;
    s.a2 = (1.0 - k / q + k2) * norm;
}

struct Butter3
{
    Biquad first, second;
    void design (bool high, double fc, double fs) { designFirst (first, high, fc, fs); designSecond (second, high, fc, fs, 1.0); }
    void reset() { first.reset(); second.reset(); }
    double run (double x) { return second.run (first.run (x)); }
};

// the learned level control: one GRU step per 48 kHz tick. The weights are transposed once so the inner loops are
// contiguous multiply-adds (they vectorise without fast-math reassociation).
struct NetWeights
{
    static constexpr int H = net::hidden, I = net::inputs;
    alignas (16) float wih[I][3 * H];
    alignas (16) float whh[H][3 * H];
    NetWeights()
    {
        for (int j = 0; j < 3 * H; ++j)
        {
            for (int k = 0; k < I; ++k) wih[k][j] = net::Wih[j * I + k];
            for (int m = 0; m < H; ++m) whh[m][j] = net::Whh[j * H + m];
        }
    }
    static const NetWeights& get() { static const NetWeights w; return w; }
};

class GainNet
{
public:
    void reset() { std::fill (h, h + net::hidden, 0.0f); out = 0.0f; }
    void step (const float* in)
    {
        constexpr int H = net::hidden, I = net::inputs;
        const auto& W = NetWeights::get();
        alignas (16) float gi[3 * H], gh[3 * H];
        static_assert ((3 * H) % 16 == 0, "blocks of 16 outputs");
        for (int jb = 0; jb < 3 * H; jb += 16)         // 16 outputs kept in registers over all inputs
        {
#if WBASSX_SIMD_SSE
            __m128 i0 = _mm_loadu_ps (net::bih + jb), i1 = _mm_loadu_ps (net::bih + jb + 4), i2 = _mm_loadu_ps (net::bih + jb + 8), i3 = _mm_loadu_ps (net::bih + jb + 12);
            for (int k = 0; k < I; ++k)
            {
                const __m128 v = _mm_set1_ps (in[k]); const float* w = &W.wih[k][jb];
                i0 = _mm_add_ps (i0, _mm_mul_ps (_mm_load_ps (w), v));      i1 = _mm_add_ps (i1, _mm_mul_ps (_mm_load_ps (w + 4), v));
                i2 = _mm_add_ps (i2, _mm_mul_ps (_mm_load_ps (w + 8), v));  i3 = _mm_add_ps (i3, _mm_mul_ps (_mm_load_ps (w + 12), v));
            }
            __m128 h0 = _mm_loadu_ps (net::bhh + jb), h1 = _mm_loadu_ps (net::bhh + jb + 4), h2 = _mm_loadu_ps (net::bhh + jb + 8), h3 = _mm_loadu_ps (net::bhh + jb + 12);
            for (int m = 0; m < H; ++m)
            {
                const __m128 v = _mm_set1_ps (h[m]); const float* w = &W.whh[m][jb];
                h0 = _mm_add_ps (h0, _mm_mul_ps (_mm_load_ps (w), v));      h1 = _mm_add_ps (h1, _mm_mul_ps (_mm_load_ps (w + 4), v));
                h2 = _mm_add_ps (h2, _mm_mul_ps (_mm_load_ps (w + 8), v));  h3 = _mm_add_ps (h3, _mm_mul_ps (_mm_load_ps (w + 12), v));
            }
            _mm_store_ps (gi + jb, i0); _mm_store_ps (gi + jb + 4, i1); _mm_store_ps (gi + jb + 8, i2); _mm_store_ps (gi + jb + 12, i3);
            _mm_store_ps (gh + jb, h0); _mm_store_ps (gh + jb + 4, h1); _mm_store_ps (gh + jb + 8, h2); _mm_store_ps (gh + jb + 12, h3);
#elif WBASSX_SIMD_NEON
            float32x4_t i0 = vld1q_f32 (net::bih + jb), i1 = vld1q_f32 (net::bih + jb + 4), i2 = vld1q_f32 (net::bih + jb + 8), i3 = vld1q_f32 (net::bih + jb + 12);
            for (int k = 0; k < I; ++k)
            {
                const float v = in[k]; const float* w = &W.wih[k][jb];
                i0 = vmlaq_n_f32 (i0, vld1q_f32 (w), v); i1 = vmlaq_n_f32 (i1, vld1q_f32 (w + 4), v);
                i2 = vmlaq_n_f32 (i2, vld1q_f32 (w + 8), v); i3 = vmlaq_n_f32 (i3, vld1q_f32 (w + 12), v);
            }
            float32x4_t h0 = vld1q_f32 (net::bhh + jb), h1 = vld1q_f32 (net::bhh + jb + 4), h2 = vld1q_f32 (net::bhh + jb + 8), h3 = vld1q_f32 (net::bhh + jb + 12);
            for (int m = 0; m < H; ++m)
            {
                const float v = h[m]; const float* w = &W.whh[m][jb];
                h0 = vmlaq_n_f32 (h0, vld1q_f32 (w), v); h1 = vmlaq_n_f32 (h1, vld1q_f32 (w + 4), v);
                h2 = vmlaq_n_f32 (h2, vld1q_f32 (w + 8), v); h3 = vmlaq_n_f32 (h3, vld1q_f32 (w + 12), v);
            }
            vst1q_f32 (gi + jb, i0); vst1q_f32 (gi + jb + 4, i1); vst1q_f32 (gi + jb + 8, i2); vst1q_f32 (gi + jb + 12, i3);
            vst1q_f32 (gh + jb, h0); vst1q_f32 (gh + jb + 4, h1); vst1q_f32 (gh + jb + 8, h2); vst1q_f32 (gh + jb + 12, h3);
#else
            float ai[16], ah[16];
            for (int q = 0; q < 16; ++q) { ai[q] = net::bih[jb + q]; ah[q] = net::bhh[jb + q]; }
            for (int k = 0; k < I; ++k) { const float v = in[k]; const float* w = &W.wih[k][jb]; for (int q = 0; q < 16; ++q) ai[q] += w[q] * v; }
            for (int m = 0; m < H; ++m) { const float v = h[m]; const float* w = &W.whh[m][jb]; for (int q = 0; q < 16; ++q) ah[q] += w[q] * v; }
            for (int q = 0; q < 16; ++q) { gi[jb + q] = ai[q]; gh[jb + q] = ah[q]; }
#endif
        }
        for (int j = 0; j < H; ++j)
        {
            const float r = 1.0f / (1.0f + std::exp (-(gi[j] + gh[j])));
            const float z = 1.0f / (1.0f + std::exp (-(gi[H + j] + gh[H + j])));
            const float n = std::tanh (gi[2 * H + j] + r * gh[2 * H + j]);
            h[j] = (1.0f - z) * n + z * h[j];
        }
        float a = net::bout;
        for (int m = 0; m < H; ++m) a += net::Wout[m] * h[m];
        out = a;
    }
    // ln g = -softplus(out)
    double lnGain() const { const double v = out; return -(v > 20.0 ? v : std::log1p (std::exp (v))); }
    float out = 0.0f;

private:
    float h[net::hidden] {};
};

class Processor
{
public:
    static constexpr double harmonicsCapDb = 10.958;     // the Harmonics fader's effect stops here (measured)

    void prepare (double sampleRate, int channels)
    {
        fs = sampleRate;
        nCh = std::clamp (channels, 1, 2);
        design();
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.lp.reset(); c.hp.reset(); c.hpB.reset(); c.hpG1.reset(); c.hpG2.reset(); c.zG1.reset(); c.zG2.reset();
            c.net.reset(); c.tNow = 0.0; c.vPrev = 0.0; std::fill (c.fPrev, c.fPrev + net::inputs, 0.0f);
        }
    }

    void setInputDb (double db) { input = db <= -99.9 ? 0.0 : std::pow (10.0, std::min (db, 0.0) / 20.0); }
    void setFreq (double hz) { hz = std::clamp (std::round (hz), 32.0, 256.0); if (hz != freq) { freq = hz; design(); } }
    void setDecayDb (double db) { decay = std::pow (10.0, std::clamp (db, -24.0, -9.0) / 20.0); }
    void setRatio (double r) { ratio = std::clamp (r, 1.0, 4.0); lnRatio = (float) std::log (ratio); }
    void setResponseMs (double ms) { response = std::clamp (ms, 10.0, 30.0); respFeat = (float) ((response - 20.0) / 10.0); }
    void setHighPass (int mode) { mode = std::clamp (mode, 0, 2); if (mode != hpMode) { hpMode = mode; design(); } }
    void setOrigBassDb (double db) { orig = db <= -99.9 ? 0.0 : std::pow (10.0, db / 20.0); }
    void setHarmonicsDb (double db) { harm = db <= -99.9 ? 0.0 : std::pow (10.0, std::min (db, harmonicsCapDb) / 20.0); }
    void setMonitor (int m) { monitor = std::clamp (m, 0, 2); }

    float lastPeak[2] {};
    double fixedGain = -1.0;     // tests only: >= 0 replaces the learned level control by this gain

    void process (float* const* io, int numChannels, int n)
    {
        const int c = std::min (numChannels, nCh);
        const double a = 8.0 * decay;
        for (int k = 0; k < c; ++k)
        {
            auto& s = ch[k];
            float peak = 0.0f;
            float* d = io[k];
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) d[i] * input;
                const double lo = s.lp.run (x), hi = s.hp.run (x);

                // the level control on its own 48 kHz clock (the rate it was trained at)
                const double z = kOut * s.zG2.run (s.zG1.run (lo));
                const float fCur[net::inputs] = { feat (x), feat (lo), lnRatio, respFeat, logF, feat (z), (float) (hpMode - 1) };
                const double tPrev = s.tNow; s.tNow += step48;
                for (double t = std::floor (tPrev) + 1.0; t <= s.tNow; t += 1.0)
                {
                    const float w = (float) ((t - tPrev) / step48);
                    float in[net::inputs];
                    for (int m = 0; m < net::inputs; ++m) in[m] = s.fPrev[m] + w * (fCur[m] - s.fPrev[m]);
                    s.net.step (in);
                }
                if (s.tNow > 1.0e6) s.tNow -= std::floor (s.tNow);
                std::copy (fCur, fCur + net::inputs, s.fPrev);
                const double g = fixedGain >= 0.0 ? fixedGain : std::exp (s.net.lnGain());

                // the Decay feedback: y = w (1 + a HP_B(y) of the previous sample), held within +-1
                const double y = std::clamp (g * lo * (1.0 + a * s.vPrev), -1.0, 1.0);
                s.vPrev = s.hpB.run (y);
                const double G = -kOut * s.hpG2.run (s.hpG1.run (y));

                double out = monitor == 1 ? G : monitor == 2 ? lo : hi + orig * lo + harm * G;
                out = std::clamp (out, -1.0, 1.0);
                d[i] = (float) out;
                peak = std::max (peak, (float) std::abs (out));
            }
            lastPeak[k] = peak;
        }
        if (c == 1) lastPeak[1] = lastPeak[0];
    }

private:
    struct Channel
    {
        Butter3 lp, hp;
        Biquad hpB, hpG1, hpG2, zG1, zG2;
        GainNet net;
        double tNow = 0.0, vPrev = 0.0;
        float fPrev[net::inputs] {};
    };
    Channel ch[2];
    double fs = 48000, freq = 80, input = 1, decay = 0.17782794, ratio = 1, response = 20, orig = 1, harm = 0, kOut = 4.53;
    double step48 = 1.0;
    float lnRatio = 0.0f, respFeat = 0.0f, logF = 0.0f;
    int hpMode = 1, monitor = 0, nCh = 2;

    static float feat (double v) { return (float) (std::abs (std::asinh (v / 1.0e-3)) / 8.0); }

    void design()
    {
        const double q = 0.70710678118654752;
        const double fB = std::clamp (freq / 3.0, 20.0, 40.0);
        for (auto& c : ch)
        {
            c.lp.design (false, freq, fs); c.hp.design (true, freq, fs);
            designSecond (c.hpB, true, fB, fs, q);
            if (hpMode == 0)      { designSecond (c.hpG1, true, 16.0, fs, q); designSecond (c.hpG2, true, 16.0, fs, q); }
            else if (hpMode == 1) { designSecond (c.hpG1, true, freq, fs, q); designSecond (c.hpG2, true, 16.0, fs, q); }
            else                  { designSecond (c.hpG1, true, 0.8 * freq, fs, q); designSecond (c.hpG2, true, 0.8 * freq, fs, q); }
            auto keep = [] (Biquad& dst, const Biquad& src) { const double z1 = dst.z1, z2 = dst.z2; dst = src; dst.z1 = z1; dst.z2 = z2; };
            keep (c.zG1, c.hpG1); keep (c.zG2, c.hpG2);
        }
        kOut = net::kOut[(int) freq - 32];
        logF = (float) std::log (freq / 80.0);
        step48 = 48000.0 / fs;
    }
};
} // namespace wbassx
