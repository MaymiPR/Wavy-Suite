#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// WBass Rider's riding (Phase 4): the reference's measured stages (MEASUREMENTS.md "WBass Rider: Phase 3 / 4"), fitted
// on its audio; nulltest/brmodel.c is the same model, line for line, and nulltest/br_fit.py fits its note detector. Per
// input sample (the mid of the input, undelayed) it returns the ride in dB, which the processor applies to the audio its
// latency later (the look-ahead).
//   the level read: E = 10 log10 of the analytic envelope's power of the input through a low shelf (31.35 Hz, -28.9 dB,
//                   Q 0.51), followed 2 ms up / 100 ms down; the law: Target - E + 0.68 dB, clamped to the range
//   the detector:   the note level N, the peak of the input low-passed near 1 kHz over the last 1024 / 2048 / 4096
//                   samples (at 48 kHz; the window by pitch band, over 172 / 78 Hz, as the reference's timings step
//                   with pitch), a block level (the peak in each quarter window, for re-attacks), the envelope of the
//                   band over 2 kHz (an illegal note), two peak followers of N: a long reference R and a slow
//                   envelope S, and a one-pole follower F of N (which lags a decaying note by a bounded amount)
//   a note:         starts over the floor, legal and within the Artifacts gate under R (for a moment
//                   after a note has ended: legal and within 38 dB of its peak); while it stays over F less a margin
//                   (Sensitivity; and for a moment after) the gain follows the law, then it holds (a slow fade is
//                   followed throughout); it is released when it stays under S
//                   by a depth (Response, Sensitivity) for a while, and a rise from there is a new note, as is (still
//                   sounding) a rise over S by a threshold (Sensitivity); it ends when it stays 38 dB under its peak, and
//                   the gain glides to the Idle value at 32 dB/s, held where it is while an ignored note sounds
namespace wbassrider
{
struct RideSettings
{
    double target = -14.0, rangeMin = -12.0, rangeMax = 12.0, idleDb = 0.0, artifacts = 50.0, sensitivity = 50.0;
    bool fast = false;
};

class Ride
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // the quadrature pair for the rate (nulltest/design_hilbert.py: 12 Hz .. 20 kHz to 0.09 .. 0.23 degrees)
        static const double a44[] = { 0.0866808661, 0.5220622640, 0.8280574169, 0.9465481697, 0.9843337230, 0.9961024007 };
        static const double b44[] = { 0.2941886855, 0.7048765504, 0.9031801816, 0.9708695148, 0.9918102724, 0.9988535508 };
        static const double a48[] = { 0.0883565679, 0.5293150639, 0.8335757261, 0.9490582015, 0.9852616074, 0.9963705210 };
        static const double b48[] = { 0.2992360867, 0.7119307996, 0.9070333667, 0.9724246851, 0.9923386306, 0.9989353286 };
        static const double a96[] = { 0.0842398305, 0.5043409134, 0.8087554235, 0.9356045791, 0.9793600281, 0.9935949019, 0.9983241816 };
        static const double b96[] = { 0.2843954979, 0.6838824961, 0.8879645284, 0.9634067099, 0.9884364222, 0.9965569751, 0.9995008064 };
        static const double a192[] = { 0.1240343698, 0.5715984706, 0.8285755469, 0.9368858037, 0.9775727627, 0.9921577806, 0.9973287974, 0.9992475872 };
        static const double b192[] = { 0.3616154560, 0.7249654229, 0.8952939748, 0.9622744304, 0.9867167286, 0.9953941894, 0.9985034598, 0.9997717334 };
        const double* a = a48; const double* b = b48; int k = 6;
        if (fs < 46000.0)       { a = a44; b = b44; }
        else if (fs > 130000.0) { a = a192; b = b192; k = 8; }
        else if (fs > 70000.0)  { a = a96; b = b96; k = 7; }
        for (auto* q : { &qe, &qh }) q->set (a, b, k);
        for (int w = 0; w < 3; ++w) win[w].prepare ((int) std::lround ((1024 << w) * fs / 48000.0));
        shelf.lowShelf (fs, 31.35, -28.90, 0.5075);
        lp.lowPass (fs, 1020.0, 0.60);
        hp.highPass (fs, 2000.0, 0.7071);
        cpa = coef (2.0); cpr = coef (100.0); cna = coef (1.0); cnr = coef (1.0); cra = coef (1.0); krr = decay (refRel);
        csa = coef (slowAtt); ksr = decay (slowRel); csm = coef (5.0); cfa = coef (followAtt); cfr = coef (followRel);
        glide = 32.0 / fs;
        relN = (int) std::lround (relMs * 1.0e-3 * fs);
        holdN = (int) std::lround (holdMs * 1.0e-3 * fs);
        offN = (int) std::lround (offMs * 1.0e-3 * fs);
        winN = (int) std::lround (winMs * 1.0e-3 * fs);
        offRelN = (int) std::lround (offRelMs * 1.0e-3 * fs);
        reset();
    }

    void reset()
    {
        shelf.clear(); lp.clear(); hp.clear(); qe.clear(); qh.clear();
        for (auto& w : win) w.clear();
        prevXl = 0.0; period = 0.01 * fs; lastCross = -1; now = 0; cyclePos = cycleNeg = lastNp = 0.0; blockMax = 0.0; Nb = -300.0; blockPos = 0;
        P = Np = Hp = Rf = Sl = Fp = 0.0;
        inNote = released = below = false; relCount = under = offCount = 0; sinceOff = 1 << 30;
        peakN = -300.0; minN = 300.0; g = 0.0; raw = out = idleNow; energy = -300.0;
    }

    // one sample of the detector's input (the mid of the undelayed input); returns the ride in dB
    double push (double x, const RideSettings& s)
    {
        idleNow = s.idleDb;
        energy = db (follow (P, qe.power (shelf.process (x)), cpa, cpr));
        const double gt = s.target - energy + 0.68;
        const double xl = lp.process (x);
        // a rising zero crossing: the period since the last one, if both halves of the cycle reached within 12 dB of the
        // note level (not a filter's ringing in silence, nor a note's last part cycle)
        if (xl > 0.0) cyclePos = std::max (cyclePos, xl * xl); else cycleNeg = std::max (cycleNeg, xl * xl);
        if (prevXl <= 0.0 && xl > 0.0)
        {
            if (lastCross >= 0 && std::min (cyclePos, cycleNeg) > 0.0631 * lastNp && (double) (now - lastCross) > 0.0004 * fs)
                period = (double) (now - lastCross);
            lastCross = now; cyclePos = cycleNeg = 0.0;
        }
        prevXl = xl;
        // the window by pitch band (measured: 1024 samples over 172 Hz, 2048 over 78 Hz, 4096 under)
        const double fz = fs / period;
        const int wsel = fz > bandHi ? 0 : (fz > bandLo ? 1 : 2);
        double m[3];
        for (int w = 0; w < 3; ++w) m[w] = win[w].push (now, xl * xl);
        ++now;
        const double np = m[wsel];
        lastNp = np;
        // the block level: the highest positive peak in each block of a quarter of the window, held from its end (it
        // wobbles where a period is longer than a block: the reference's re-rides just over each band's edge)
        blockMax = std::max (blockMax, xl);
        if (++blockPos >= win[wsel].length / 4) { Nb = 20.0 * std::log10 (blockMax + 1.0e-15); blockMax = 0.0; blockPos = 0; }
        const double N = db (np);
        const double H = db (follow (Hp, qh.power (hp.process (x)), cna, cnr));
        const double R = db (peak (Rf, np, cra, krr));
        const double S = db (peak (Sl, np, csa, ksr));
        const double F = db (follow (Fp, np, cfa, cfr));
        const bool open = N > floorDb && H < N && N > R - (20.0 - 0.17 * s.artifacts);
        bool start = false;
        // for a moment after a note ends, any legal note within 38 dB of its peak is a new note (no floor, no Artifacts
        // gate); then the gates apply again
        if (! inNote) { start = sinceOff < winN ? (H < N && N > peakN - 38.0) : open; ++sinceOff; }
        else
        {
            peakN = std::max (peakN, N);
            offCount = (N < peakN - 38.0 || N < floorDb - 3.0) ? offCount + 1 : 0;
            if (offCount > (released ? offRelN : offN)) { inNote = false; sinceOff = 0; }   // (a released note: its own delay)
            else if (! released)
            {
                relCount = N < S - releaseDepth (s) ? relCount + 1 : 0;
                if (relCount >= relN) { released = true; minN = Nb; }
                const double thr = onThreshold (s);
                if (N < S + thr - hyst) below = true;
                if (! released && below && open && N > S + thr) start = true;
            }
            else
            {
                minN = std::min (minN, Nb);                    // (a rise is read, and gated, on the block level)
                const bool openB = Nb > floorDb && H < N && Nb > R - (20.0 - 0.17 * s.artifacts);
                if (openB && Nb > minN + onRise) start = true;
            }
        }
        if (start) { inNote = true; released = below = false; relCount = under = offCount = 0; peakN = N; g = gt; }
        else if (inNote)
        {
            under = N > F + bySensitivity (margin, s.sensitivity) ? 0 : under + 1;
            if (under < holdN) g = gt;                     // (then the gain holds where it is, whatever the level does)
        }
        if (inNote) raw = std::clamp (g, s.rangeMin, s.rangeMax);
        else if (N < peakN - 38.0)          // silence (38 dB under the last note): the glide to the Idle value
            raw = raw > s.idleDb ? std::max (s.idleDb, raw - glide) : std::min (s.idleDb, raw + glide);
        // (an ignored note, within 38 dB of the last: the gain stays where it is)
        out = raw + csm * (out - raw);
        return out;
    }

    double energy = -300.0;          // the level read (dB), what the Energy meter shows

    // the note detector's constants (nulltest/br_fit.py, measurements/wbassrider/detector_fit.json; br_export.py)
    double slowAtt = 8.9595, slowRel = 1278.0703, refRel = 5678.4626, hyst = 0.0033, relMs = 1.1049, holdMs = 67.3689, onRise = 0.0030,
           followAtt = 6.5472, followRel = 365.3028, bandHi = 172.0, bandLo = 78.0, offMs = 27.0968, offRelMs = 7.0273, winMs = 1.1054, floorDb = -48.75;
    // at Sensitivity 0 .. 100 by 25: the rise over S that is a new note, and the margin over F that keeps the attack
    double onThr[5] { 1.3976, 0.4913, 1.4605, 3.2098, -0.8084 }, margin[5] { 1.5000, 1.9344, 0.2314, 0.0099, -0.8320 };
    double depthSlow[5] { 12.7293, 12.7678, 13.0000, 12.1833, 6.1958 }, depthFast[5] { 4.6770, 5.7842, 4.3622, 4.0200, 3.6643 };

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() { z1 = z2 = 0.0; }
        void set (double c0, double c1, double c2, double d0, double d1, double d2) { b0 = c0 / d0; b1 = c1 / d0; b2 = c2 / d0; a1 = d1 / d0; a2 = d2 / d0; }
        void lowShelf (double fs, double fc, double gdb, double q)
        {
            const double A = std::pow (10.0, gdb / 40.0), w0 = 2.0 * 3.141592653589793 * fc / fs, c = std::cos (w0), al = std::sin (w0) / (2.0 * q), sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
        void lowPass (double fs, double fc, double q)
        {
            const double w0 = 2.0 * 3.141592653589793 * fc / fs, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void highPass (double fs, double fc, double q)
        {
            const double w0 = 2.0 * 3.141592653589793 * fc / fs, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
    };

    // the maximum over the last `length` samples (a monotonic queue)
    struct SlidingMax
    {
        std::vector<double> v; std::vector<long long> t; int cap = 0, head = 0, tail = 0, length = 1;
        void prepare (int len) { length = std::max (1, len); cap = length + 2; v.assign ((size_t) cap, 0.0); t.assign ((size_t) cap, 0); clear(); }
        void clear() { head = tail = 0; }
        double push (long long at, double x)
        {
            while (tail != head && v[(size_t) ((tail - 1 + cap) % cap)] <= x) tail = (tail - 1 + cap) % cap;
            v[(size_t) tail] = x; t[(size_t) tail] = at; tail = (tail + 1) % cap;
            while (t[(size_t) head] <= at - length) head = (head + 1) % cap;
            return v[(size_t) head];
        }
    };

    // two chains of second-order allpass sections in z^-2, the second one sample later: |a + j b|^2
    struct Quad
    {
        const double* ca = nullptr; const double* cb = nullptr; int k = 0;
        double ax1[8] {}, ax2[8] {}, ay1[8] {}, ay2[8] {}, bx1[8] {}, bx2[8] {}, by1[8] {}, by2[8] {}, d = 0.0;
        void set (const double* a, const double* b, int n) { ca = a; cb = b; k = n; clear(); }
        void clear()
        {
            for (auto* v : { ax1, ax2, ay1, ay2, bx1, bx2, by1, by2 }) std::memset (v, 0, sizeof (double) * 8);
            d = 0.0;
        }
        static double chain (const double* c, int k, double* x1, double* x2, double* y1, double* y2, double x)
        {
            for (int i = 0; i < k; ++i)
            {
                const double y = c[i] * (x + y2[i]) - x2[i];
                x2[i] = x1[i]; x1[i] = x; y2[i] = y1[i]; y1[i] = y;
                x = y;
            }
            return x;
        }
        double power (double x)
        {
            const double ya = chain (ca, k, ax1, ax2, ay1, ay2, x), yb = chain (cb, k, bx1, bx2, by1, by2, d);
            d = x;
            return ya * ya + yb * yb;
        }
    };

    double coef (double ms) const { return ms <= 0.0 ? 0.0 : std::exp (-1.0 / (ms * 1.0e-3 * fs)); }
    double decay (double ms) const { return std::exp (-2.0 / (ms * 1.0e-3 * fs)); }      // an amplitude time constant, on power
    static double follow (double& s, double v, double ca, double cr) { const double c = v > s ? ca : cr; s += (1.0 - c) * (v - s); return s; }
    // a peak follower on power: a one-pole rise, a fall no faster than kd per sample, anything slower tracked exactly
    static double peak (double& s, double v, double ca, double kd) { if (v > s) s += (1.0 - ca) * (v - s); else s = std::max (v, s * kd); return s; }
    static double db (double p) { return 10.0 * std::log10 (p + 1.0e-30); }

    static double bySensitivity (const double* d, double sensitivity)
    {
        const double t = std::clamp (sensitivity / 25.0, 0.0, 3.999);
        const int i = (int) t;
        return d[i] + (d[i + 1] - d[i]) * (t - i);
    }
    double releaseDepth (const RideSettings& s) const { return bySensitivity (s.fast ? depthFast : depthSlow, s.sensitivity); }
    double onThreshold (const RideSettings& s) const  { return bySensitivity (onThr, s.sensitivity); }

    double fs = 48000.0;
    Biquad shelf, lp, hp;
    Quad qe, qh;
    SlidingMax win[3];
    double prevXl = 0.0, period = 480.0, cyclePos = 0.0, cycleNeg = 0.0, lastNp = 0.0, blockMax = 0.0, Nb = -300.0;
    int blockPos = 0;
    long long lastCross = -1, now = 0;
    double cpa = 0, cpr = 0, cna = 0, cnr = 0, cra = 0, krr = 0, csa = 0, ksr = 0, csm = 0, cfa = 0, cfr = 0, glide = 0;
    int relN = 0, holdN = 0, offN = 0, offRelN = 0, winN = 0, sinceOff = 1 << 30;
    double P = 0, Np = 0, Hp = 0, Rf = 0, Sl = 0, Fp = 0;
    bool inNote = false, released = false, below = false;
    int relCount = 0, under = 0, offCount = 0;
    double peakN = -300.0, minN = 300.0, g = 0.0, raw = 0.0, out = 0.0, idleNow = 0.0;
};
}
