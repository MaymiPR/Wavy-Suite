#pragma once
// WComp's compressor, as identified on the reference (MEASUREMENTS.md "WComp", nulltest/ident_hc_*.py).
//
//   detector  the larger channel's |x|, held over the next 64 samples (the look-ahead; Punch delays it by 1 ms per unit)
//   static    GR = h((1 - 1/ratio) * max(0, 20 log10(peak + 0.001) - threshold)), h a measured table
//   release   in the gain-reduction domain: a slow state S follows the output (charging at the Crest time constant)
//             and releases at fs(S) / release; the output R falls at the same law with an effective release
//             min(release, release * w0(S), K / (R - S)^p), much shorter after short events
//   attack    a one-pole on the linear gain (falling only), time constant = Attack
//   smoothing two box filters (38 and 27 samples), then makeup, the dry / wet mix and the output gain
//   limiter   a 64-sample look-ahead peak limiter on the output (Off: as the reference's "clip", On: held longer)
// All sizes in samples are for 44.1 / 48 kHz and scale by 2 / 4 at 88.2-96 / 176.4-192 kHz (the latency does).
#include <algorithm>
#include <cmath>
#include <vector>
#include "WCompTables.h"

namespace wcomp
{
class Compressor
{
public:
    struct Settings
    {
        float thresholdDb = -18.0f, ratio = 3.0f, attackMs = 7.0f, releaseMs = 100.0f, punch = 1.0f, mix = 1.0f, outputDb = 0.0f;
        bool limiter = false;
    };

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        scale = fs > 100000.0 ? 4 : fs > 50000.0 ? 2 : 1;
        la = 64 * scale;
        channels = std::max (1, std::min (2, numChannels));
        maxPunch = (int) std::ceil (30.0 * 1e-3 * fs) + 1;
        delay.assign ((size_t) channels, std::vector<float> ((size_t) la, 0.0f));
        det.assign ((size_t) (la + maxPunch + 2), 0.0f);
        b1.assign ((size_t) (38 * scale), 1.0f); b2.assign ((size_t) (27 * scale), 1.0f);
        lb1.assign ((size_t) (38 * scale), 1.0f); lb2.assign ((size_t) (27 * scale), 1.0f);
        lim.assign ((size_t) (la + 8 * scale + 2), 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (det.begin(), det.end(), 0.0f);
        std::fill (lim.begin(), lim.end(), 0.0f);
        std::fill (b1.begin(), b1.end(), 1.0f); std::fill (b2.begin(), b2.end(), 1.0f);
        std::fill (lb1.begin(), lb1.end(), 1.0f); std::fill (lb2.begin(), lb2.end(), 1.0f);
        sum1 = (double) b1.size(); sum2 = (double) b2.size(); lsum1 = (double) lb1.size(); lsum2 = (double) lb2.size();
        i1 = i2 = li1 = li2 = 0;
        pos = 0; detPos = 0; limPos = 0;
        R = S = 0.0; g1 = 1.0;
    }

    int latency() const { return la; }

    void setSettings (const Settings& s)
    {
        set = s;
        slope = 1.0 - 1.0 / std::max (1.0, (double) s.ratio);
        attC = std::exp (-1.0 / (0.99 * std::max (0.05, (double) s.attackMs) * 1e-3 * fs));
        relS = std::max (1e-3, (double) s.releaseMs * 1e-3);
        crestC = std::exp (-1.0 / ((double) tables::crestMs * 1e-3 * fs));
        if constexpr (tables::hasRelease3) crest3C = std::exp (-1.0 / ((double) tables::crest3Ms * 1e-3 * fs));
        punchSamples = std::clamp ((int) std::lround (s.punch * 1e-3 * fs), 0, maxPunch - 1);
        makeup = std::pow (10.0, makeupDb (s.thresholdDb, s.ratio) * makeupScale (s.releaseMs) / 20.0);
        outGain = std::pow (10.0, s.outputDb / 20.0);
    }

    static double makeupDb (double thr, double ratio)
    {
        using namespace tables;
        double k;
        if (ratio <= makeupRatio[0]) k = makeupSlope[0];
        else if (ratio >= makeupRatio[makeupCount - 1]) k = makeupSlope[makeupCount - 1];
        else
        {
            int i = 0;
            while (ratio > makeupRatio[i + 1]) ++i;
            // interpolate in 1 / ratio (the slope follows 1 - 1/ratio closely)
            const double a = 1.0 / makeupRatio[i], b = 1.0 / makeupRatio[i + 1], f = (1.0 / ratio - a) / (b - a);
            k = makeupSlope[i] + f * (makeupSlope[i + 1] - makeupSlope[i]);
        }
        return k * std::max (0.0, -9.0 - thr);
    }

    // the makeup measured at Release 100 ms scales with the release time (the same factor for every threshold and ratio)
    static double makeupScale (double releaseMs)
    {
        using namespace tables;
        const double r = std::log (std::clamp (releaseMs, (double) makeupRelMs[0], (double) makeupRelMs[makeupRelCount - 1]));
        int i = 0;
        while (i < makeupRelCount - 2 && r > std::log ((double) makeupRelMs[i + 1])) ++i;
        const double a = std::log ((double) makeupRelMs[i]), b = std::log ((double) makeupRelMs[i + 1]), f = (r - a) / (b - a);
        return makeupRelScale[i] + f * (makeupRelScale[i + 1] - makeupRelScale[i]);
    }

    // in-place; ch[0..channels-1]
    void process (float* const* ch, int n)
    {
        minGain = minLimGain = 1.0;
        for (int i = 0; i < n; ++i)
        {
            // input side: the detector level and the delayed audio
            float a = 0.0f;
            for (int c = 0; c < channels; ++c) a = std::max (a, std::abs (ch[c][i]));
            det[(size_t) detPos] = a;
            const double h = heldPeak();
            const double Rt = targetGr (h);
            stepRelease (Rt);
            const double gt = std::pow (10.0, -R / 20.0);
            g1 = gt < g1 ? gt + attC * (g1 - gt) : gt;
            sum1 += g1 - b1[(size_t) i1]; b1[(size_t) i1] = (float) g1; i1 = (i1 + 1) % (int) b1.size();
            const double v1 = sum1 / (double) b1.size();
            sum2 += v1 - b2[(size_t) i2]; b2[(size_t) i2] = (float) v1; i2 = (i2 + 1) % (int) b2.size();
            const double g = sum2 / (double) b2.size();
            minGain = std::min (minGain, g);

            // the output limiter sees the incoming samples at the current gain (what they will become)
            const double gTarget = std::pow (10.0, -Rt / 20.0);
            const double gEst = limEstimate == 1 ? gTarget : limEstimate == 2 ? std::max (g, gTarget) : g;
            const double wetGain = (1.0 - set.mix) + set.mix * gEst * makeup;
            const double wetNow = (1.0 - set.mix) + set.mix * g * makeup;
            lim[(size_t) limPos] = (float) (limEstimate == 3 ? a : a * wetGain * outGain);
            const double lh = limEstimate == 3 ? limiterHeld() * wetNow * outGain : limiterHeld();
            const double lt = lh > 1.0 ? 1.0 / lh : 1.0;
            lsum1 += lt - lb1[(size_t) li1]; lb1[(size_t) li1] = (float) lt; li1 = (li1 + 1) % (int) lb1.size();
            const double lv = lsum1 / (double) lb1.size();
            lsum2 += lv - lb2[(size_t) li2]; lb2[(size_t) li2] = (float) lv; li2 = (li2 + 1) % (int) lb2.size();
            const double lg = lsum2 / (double) lb2.size();
            minLimGain = std::min (minLimGain, lg);

            for (int c = 0; c < channels; ++c)
            {
                auto& d = delay[(size_t) c];
                const float x = d[(size_t) pos];
                d[(size_t) pos] = ch[c][i];
                const double y = ((1.0 - set.mix) * x + set.mix * x * g * makeup) * outGain * lg;
                ch[c][i] = (float) y;
            }
            pos = (pos + 1) % la;                      // the delay line returns the input of la samples ago
            detPos = (detPos + 1) % (int) det.size();
            limPos = (limPos + 1) % (int) lim.size();
        }
    }

    double gainReductionDb() const { return R; }
    int limEstimate = 3;                               // how the limiter estimates the output (3: the current gain times the pending peak; 0-2 tried, worse)
    // the deepest reduction of the smoothed compressor gain during the last process() call (dB, positive)
    double blockGainReductionDb() const { return minGain < 1.0 ? -20.0 * std::log10 (std::max (minGain, 1e-6)) : 0.0; }
    // the same for the output limiter
    double blockLimiterDb() const { return minLimGain < 1.0 ? -20.0 * std::log10 (std::max (minLimGain, 1e-6)) : 0.0; }

private:
    // max of the detector over the 64 newest samples, delayed by Punch (the output is la samples behind the input)
    double heldPeak() const
    {
        const int N = (int) det.size();
        double m = 0.0;
        for (int k = 0; k < la; ++k)
        {
            int j = detPos - punchSamples - k;
            j = ((j % N) + N) % N;
            m = std::max (m, (double) det[(size_t) j]);
        }
        return m;
    }

    double limiterHeld() const
    {
        const int N = (int) lim.size(), span = la + (set.limiter ? 7 * scale : 0);
        double m = 0.0;
        for (int k = 0; k < span; ++k)
        {
            int j = limPos - k;
            j = ((j % N) + N) % N;
            m = std::max (m, (double) lim[(size_t) j]);
        }
        return m;
    }

    double targetGr (double h) const
    {
        using namespace tables;
        const double x = slope * std::max (0.0, 20.0 * std::log10 (h + floorLevel) - set.thresholdDb);
        if (x <= 0.0) return 0.0;
        if (x >= staticX[staticCount - 1]) return x - (staticX[staticCount - 1] - staticH[staticCount - 1]);
        // the table is uniform within three ranges; a short search is cheap enough
        int lo = 0, hi = staticCount - 1;
        while (hi - lo > 1) { const int m = (lo + hi) / 2; (x < staticX[m] ? hi : lo) = m; }
        const double f = (x - staticX[lo]) / (staticX[hi] - staticX[lo]);
        return staticH[lo] + f * (staticH[hi] - staticH[lo]);
    }

    static double table (const float* knot, const float* logv, int n, double x)
    {
        if (x <= knot[0]) return std::exp ((double) logv[0]);
        if (x >= knot[n - 1]) return std::exp ((double) logv[n - 1]);
        int i = 0;
        while (x > knot[i + 1]) ++i;
        const double f = (x - knot[i]) / (knot[i + 1] - knot[i]);
        return std::exp (logv[i] + f * (logv[i + 1] - logv[i]));
    }

    static double grid (double R, double S)
    {
        using namespace tables;
        if constexpr (hasRelease3)
        {
            R = std::clamp (R, (double) ff3RKnot[0], (double) ff3RKnot[ff3R - 1]);
            S = std::clamp (S, (double) ff3SKnot[0], (double) ff3SKnot[ff3S - 1]);
            int i = 0, j = 0;
            while (i < ff3R - 2 && R > ff3RKnot[i + 1]) ++i;
            while (j < ff3S - 2 && S > ff3SKnot[j + 1]) ++j;
            const double fr = (R - ff3RKnot[i]) / (ff3RKnot[i + 1] - ff3RKnot[i]), fq = (S - ff3SKnot[j]) / (ff3SKnot[j + 1] - ff3SKnot[j]);
            const auto v = [] (int a, int b) { return (double) ff3Log[a * ff3S + b]; };
            return std::exp ((1 - fr) * (1 - fq) * v (i, j) + fr * (1 - fq) * v (i + 1, j) + (1 - fr) * fq * v (i, j + 1) + fr * fq * v (i + 1, j + 1));
        }
        else { (void) R; (void) S; return 0.0; }
    }

    // the third form (ident_hc_rel3.py): the slow law for both states, plus a fast part for R that depends on how deep
    // R is and how far the slow state has caught up
    void stepRelease3 (double t)
    {
        using namespace tables;
        if constexpr (hasRelease3)
        {
            const double dt = 1.0 / fs;
            const double src = std::max (R, t);
            if (src > S)
            {
                const double gs = std::pow (10.0, -S / 20.0), gt = std::pow (10.0, -src / 20.0);
                S = std::min (src, -20.0 * std::log10 (gt + crest3C * (gs - gt)));
            }
            else { S -= (table (fs3Knot, fs3Log, fs3Count, S) / relS + table (as3Knot, as3Log, as3Count, S)) * dt; S = std::max (S, t); }
            S = std::max (S, 0.0);
            R -= (table (fs3Knot, fs3Log, fs3Count, R) / relS + table (as3Knot, as3Log, as3Count, R) + (R > S ? grid (R, S) : 0.0)) * dt;
            R = std::max ({ R, S, t });
        }
    }

    void stepRelease (double t)
    {
        using namespace tables;
        if constexpr (hasRelease3) { stepRelease3 (t); return; }
        const double dt = 1.0 / fs;
        // the slow state charges toward the output (a one-pole on the linear gain) or releases with the slow law
        const double src = std::max (R, t);
        if (src > S)
        {
            const double gs = std::pow (10.0, -S / 20.0), gt = std::pow (10.0, -src / 20.0);
            S = std::min (src, -20.0 * std::log10 (gt + crestC * (gs - gt)));
        }
        else { S -= (table (fsKnot, fsLog, fsCount, S) / relS + table (asKnot, asLog, asCount, S)) * dt; S = std::max (S, t); }
        S = std::max (S, 0.0);
        double re = relS;
        if (R - S > 1e-9)
            re = std::min ({ re, relS * table (w0Knot, w0Log, w0Count, S), relSeedMs * 1e-3 / std::pow (R - S, (double) relSeedExp) });
        R -= (table (fsKnot, fsLog, fsCount, R) / re + table (asKnot, asLog, asCount, R)) * dt;
        R = std::max (R, S);
        R = std::max (R, t);
    }

    Settings set;
    double fs = 48000.0;
    int scale = 1, la = 64, channels = 2, maxPunch = 1441, punchSamples = 48;
    double slope = 2.0 / 3.0, attC = 0.0, relS = 0.1, crestC = 0.0, crest3C = 0.0, makeup = 1.0, outGain = 1.0;
    std::vector<std::vector<float>> delay;
    std::vector<float> det, lim, b1, b2, lb1, lb2;
    double sum1 = 0.0, sum2 = 0.0, lsum1 = 0.0, lsum2 = 0.0;
    int i1 = 0, i2 = 0, li1 = 0, li2 = 0, pos = 0, detPos = 0, limPos = 0;
    double R = 0.0, S = 0.0, g1 = 1.0, minGain = 1.0, minLimGain = 1.0;
};
}
