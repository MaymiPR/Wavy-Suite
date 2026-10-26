/* WBass Rider's riding model (Phase 4), for fitting against the reference: the measured stages of
   MEASUREMENTS.md "WBass Rider: Phase 3" with every constant a parameter. Built by build_brmodel.bat into
   brmodel.dll and driven by brmodel.py.

   br_run (x, n, fs, p, gain, trace): x the detector's input (the mid of the input, undelayed), p the parameters
   (BR_* below), gain the ride in dB per input sample (the audio is delayed by the look-ahead before it is applied, so
   the gain at sample t meets the input sample t - lookahead), trace (n x 4, may be null): E dB, detector dB, state,
   the unsmoothed gain. Trace (n x 6): E, N, state (1 in a note, 2 released, 4 below, 8 a start), raw gain, S, R. */
#include <math.h>
#include <string.h>
#include <stdlib.h>

enum {
    BR_TARGET, BR_RMIN, BR_RMAX, BR_IDLE, BR_C0,
    BR_SH_FC, BR_SH_G, BR_SH_Q, BR_PB_ATT, BR_PB_REL,
    BR_LP_FC, BR_LP_Q, BR_N_ATT, BR_N_REL, BR_FLOOR,
    BR_OFF, BR_ART, BR_REL_DEPTH, BR_REL_MS, BR_GLIDE,
    BR_SMOOTH, BR_REF_ATT, BR_REF_REL, BR_HP_FC, BR_ILLEGAL,
    BR_ON_RISE, BR_SLOW_ATT, BR_SLOW_REL, BR_MODE, BR_SLOW_REL_FAST,
    BR_THR_U, BR_THR_A, BR_HYST, BR_SETTLE, BR_OFF_MS, BR_WIN_MS, BR_OFF_REL_MS, BR_NMODE, BR_PITCH_C, BR_NPARAMS
};

typedef struct { double b0, b1, b2, a1, a2, z1, z2; } Biquad;
static double bq (Biquad* f, double x)
{
    double y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}
static void bq_set (Biquad* f, double b0, double b1, double b2, double a0, double a1, double a2)
{
    f->b0 = b0 / a0; f->b1 = b1 / a0; f->b2 = b2 / a0; f->a1 = a1 / a0; f->a2 = a2 / a0; f->z1 = f->z2 = 0.0;
}
static void lowshelf (Biquad* f, double fs, double fc, double gdb, double q)
{
    double A = pow (10.0, gdb / 40.0), w0 = 2.0 * M_PI * fc / fs, c = cos (w0), al = sin (w0) / (2.0 * q), sa = 2.0 * sqrt (A) * al;
    bq_set (f, A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
            (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}
static void lowpass (Biquad* f, double fs, double fc, double q)
{
    double w0 = 2.0 * M_PI * fc / fs, c = cos (w0), al = sin (w0) / (2.0 * q);
    bq_set (f, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
static void highpass (Biquad* f, double fs, double fc, double q)
{
    double w0 = 2.0 * M_PI * fc / fs, c = cos (w0), al = sin (w0) / (2.0 * q);
    bq_set (f, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

/* the quadrature pair (design_hilbert.py: 6 + 6 sections, 0.23 degrees over 12 Hz .. 20 kHz at 48 kHz) */
static const double HA[6] = { 0.0883565679, 0.5293150639, 0.8335757261, 0.9490582015, 0.9852616074, 0.9963705210 };
static const double HB[6] = { 0.2992360867, 0.7119307996, 0.9070333667, 0.9724246851, 0.9923386306, 0.9989353286 };
typedef struct { double x1[6], x2[6], y1[6], y2[6], d; } Chain;
static double chain (Chain* s, const double* c, double x)
{
    for (int i = 0; i < 6; ++i)
    {
        double y = c[i] * (x + s->y2[i]) - s->x2[i];
        s->x2[i] = s->x1[i]; s->x1[i] = x; s->y2[i] = s->y1[i]; s->y1[i] = y;
        x = y;
    }
    return x;
}
typedef struct { Chain a, b; double d; } Quad;
static double quad_pow (Quad* q, double x)     /* |analytic|^2 of x */
{
    double ya = chain (&q->a, HA, x), yb = chain (&q->b, HB, q->d);
    q->d = x;
    return ya * ya + yb * yb;
}

static double coef (double ms, double fs) { return ms <= 0.0 ? 0.0 : exp (-1.0 / (ms * 1e-3 * fs)); }
static double follow (double* s, double v, double ca, double cr) { double c = v > *s ? ca : cr; *s += (1.0 - c) * (v - *s); return *s; }
/* a peak follower on power: a one-pole rise, and a fall no faster than the decay factor kd (per sample) but tracking
   anything slower exactly (max (v, s kd)) */
static double peak (double* s, double v, double ca, double kd)
{
    if (v > *s) *s += (1.0 - ca) * (v - *s);
    else *s = fmax (v, *s * kd);
    return *s;
}
static double decay (double ms, double fs) { return exp (-2.0 / (ms * 1e-3 * fs)); }   /* an amplitude time constant, on power */
static double db_pow (double p) { return 10.0 * log10 (p + 1e-30); }

/* a sliding maximum over the last w samples (a monotonic queue in a ring of size cap) */
typedef struct { double* v; int* t; int cap, head, tail; } SMax;
static void smax_init (SMax* q, double* vbuf, int* tbuf, int cap) { q->v = vbuf; q->t = tbuf; q->cap = cap; q->head = q->tail = 0; }
static double smax_push (SMax* q, int now, double x, int w)
{
    while (q->tail != q->head && q->v[(q->tail - 1 + q->cap) % q->cap] <= x) q->tail = (q->tail - 1 + q->cap) % q->cap;
    q->v[q->tail] = x; q->t[q->tail] = now; q->tail = (q->tail + 1) % q->cap;
    while (q->t[q->head] <= now - w) q->head = (q->head + 1) % q->cap;
    return q->v[q->head];
}
#define SMAX_CAP 16384

__declspec(dllexport) int br_nparams (void) { return BR_NPARAMS; }

__declspec(dllexport) void br_run (const float* x, int n, double fs, const double* p, float* gain, float* trace)
{
    Biquad shelf, lp, hp;
    lowshelf (&shelf, fs, p[BR_SH_FC], p[BR_SH_G], p[BR_SH_Q]);
    lowpass (&lp, fs, p[BR_LP_FC], p[BR_LP_Q]);
    highpass (&hp, fs, p[BR_HP_FC], 0.7071);
    Quad qe, qd, qh; memset (&qe, 0, sizeof qe); memset (&qd, 0, sizeof qd); memset (&qh, 0, sizeof qh);
    const double cpa = coef (p[BR_PB_ATT], fs), cpr = coef (p[BR_PB_REL], fs);
    const double cna = coef (p[BR_N_ATT], fs), cnr = coef (p[BR_N_REL], fs);
    const double cra = coef (p[BR_REF_ATT], fs), krr = decay (p[BR_REF_REL], fs);
    const double csa = coef (p[BR_SLOW_ATT], fs), ksr = decay (p[BR_SLOW_REL], fs);
    const double csm = coef (p[BR_SMOOTH], fs), chs = coef (p[BR_MODE], fs);
    const double glide = p[BR_GLIDE] / fs;
    const int relN = (int) (p[BR_REL_MS] * 1e-3 * fs + 0.5);
    const int settleN = (int) (p[BR_SETTLE] * 1e-3 * fs + 0.5), offN = (int) (p[BR_OFF_MS] * 1e-3 * fs + 0.5);
    int offCount = 0, sinceOff = 1 << 30;
    const int winN = (int) (p[BR_WIN_MS] * 1e-3 * fs + 0.5), offRelN = (int) (p[BR_OFF_REL_MS] * 1e-3 * fs + 0.5);
    double P = 0.0, Np = 0.0, Hp = 0.0, Rf = 0.0, Sl = 0.0;
    /* the windowed note level (N mode 1): sliding maxima of the low-passed waveform's square over 1024 / 2048 / 4096
       samples (at 48 kHz), the window picked by the pitch (the smallest spanning pitch_c periods) */
    double* qv = (double*) malloc (sizeof (double) * 3 * SMAX_CAP); int* qt = (int*) malloc (sizeof (int) * 3 * SMAX_CAP);
    SMax qs[3]; for (int k = 0; k < 3; ++k) smax_init (&qs[k], qv + k * SMAX_CAP, qt + k * SMAX_CAP, SMAX_CAP);
    const int wlen[3] = { (int) (1024 * fs / 48000.0 + 0.5), (int) (2048 * fs / 48000.0 + 0.5), (int) (4096 * fs / 48000.0 + 0.5) };
    double prevXl = 0.0, period = 0.01 * fs; int lastCross = -1, wsel = 0;
    int inNote = 0, released = 0, relCount = 0, below = 0, settle = 0, under = 0;
    double peakN = -300.0, g = 0.0, minN = 300.0;
    double raw = p[BR_IDLE], out = p[BR_IDLE], gts = p[BR_RMAX];
    for (int i = 0; i < n; ++i)
    {
        const double xi = x[i];
        /* the level read and the law */
        const double E = db_pow (follow (&P, quad_pow (&qe, bq (&shelf, xi)), cpa, cpr));
        double gt = p[BR_TARGET] - E + p[BR_C0];
        if (p[BR_MODE] > 0.0) { gts = gt + chs * (gts - gt); gt = gts; }   /* mode > 0: the hold acts on the law smoothed over mode ms */
        /* the detector: the low-passed note envelope, the illegal band, the long reference and the slow envelope (peak
           followers, falling at most 1.74 / 8.69 dB/s at 5000 / 1000 ms) */
        const double xl = bq (&lp, xi);
        const double dp = quad_pow (&qd, xl);
        double N;
        if (p[BR_NMODE] >= 1.0)
        {
            if (prevXl <= 0.0 && xl > 0.0)                     /* a rising zero crossing: the period since the last one */
            {
                if (lastCross >= 0) { const double pd = (double) (i - lastCross); if (pd > 0.0004 * fs) period = pd; }
                lastCross = i;
            }
            prevXl = xl;
            const double want = p[BR_PITCH_C] * period;
            wsel = want <= wlen[0] ? 0 : (want <= wlen[1] ? 1 : 2);
            double m[3];
            for (int k = 0; k < 3; ++k) m[k] = smax_push (&qs[k], i, xl * xl, wlen[k]);
            N = db_pow (m[wsel]);
        }
        else N = db_pow (follow (&Np, dp, cna, cnr));
        const double H = db_pow (follow (&Hp, quad_pow (&qh, bq (&hp, xi)), cna, cnr));
        /* the long reference and the slow envelope: peak followers of the quadrature power (env mode 0) or of the
           waveform's own peaks (env mode 1: no onset overshoot; BR_SLOW_REL_FAST) */
        const double pk = p[BR_NMODE] >= 1.0 ? pow (10.0, N / 10.0) : (p[BR_SLOW_REL_FAST] >= 1.0 ? xl * xl : dp);
        const double R = db_pow (peak (&Rf, pk, cra, krr));
        const double S = db_pow (peak (&Sl, pk, csa, ksr));
        const int legal = H < N + p[BR_ILLEGAL];
        const int open = N > p[BR_FLOOR] && legal && N > R - p[BR_ART];
        int start = 0;
        /* for a moment after a note ends, any legal note within 38 dB of its peak is a new note (no floor, no Artifacts
           gate); then the gates apply again */
        if (! inNote) { start = sinceOff < winN ? (legal && N > peakN - p[BR_OFF]) : open; ++sinceOff; }
        else
        {
            if (N > peakN) peakN = N;
            /* the note ends when it has stayed 38 dB under its peak (or under the floor) for long enough: a while if it
               was still sounding, at once if it had been released */
            offCount = (N < peakN - p[BR_OFF] || N < p[BR_FLOOR] - 3.0) ? offCount + 1 : 0;
            if (offCount > (released ? offRelN : offN)) { inNote = 0; sinceOff = 0; }
            else if (! released)
            {
                /* released: under the slow envelope by the depth for long enough */
                relCount = N < S - p[BR_REL_DEPTH] ? relCount + 1 : 0;
                if (relCount >= relN) { released = 1; minN = N; }
                /* or still sounding: a rising crossing of the slow envelope by the threshold */
                if (N < S + p[BR_THR_U] - p[BR_HYST]) below = 1;
                if (! released && below && open && N > S + p[BR_THR_U]) start = 1;
            }
            else
            {
                /* released: a rise from the lowest since is a new note (within the gates) */
                if (N < minN) minN = N;
                if (open && N > minN + p[BR_ON_RISE]) start = 1;
            }
        }
        if (start) { inNote = 1; released = 0; relCount = 0; below = 0; peakN = N; g = gt; settle = settleN; under = 0; offCount = 0; }
        else if (inNote)
        {
            /* the attack (the note over its slow envelope, which is still rising): the gain follows the law; then it
               only goes down */
            under = N > S + p[BR_THR_A] ? 0 : under + 1;
            if (under < settleN) g = gt;                 /* (BR_SETTLE: how long under it before the hold, ms) */
            else if (gt < g) g = gt;
        }
        if (inNote)
            raw = fmin (p[BR_RMAX], fmax (p[BR_RMIN], g));
        else if (N < peakN - p[BR_OFF])     /* silence (38 dB under the last note): the glide to the Idle value */
            raw = raw > p[BR_IDLE] ? fmax (p[BR_IDLE], raw - glide) : fmin (p[BR_IDLE], raw + glide);
        /* (an ignored note, within 38 dB of the last: the gain stays where it is) */
        out = raw + csm * (out - raw);
        gain[i] = (float) out;
        if (trace) { float* t = trace + 6 * i; t[0] = (float) E; t[1] = (float) N; t[2] = (float) (inNote + 2 * released + 4 * below + 8 * start); t[3] = (float) raw; t[4] = (float) S; t[5] = (float) R; }
    }
    free (qv); free (qt);
}
