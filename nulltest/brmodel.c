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
    BR_THR_U, BR_THR_A, BR_HYST, BR_SETTLE, BR_OFF_MS, BR_WIN_MS, BR_OFF_REL_MS, BR_NMODE, BR_PITCH_C, BR_HOLD_MIN, BR_WIN_LATCH, BR_FAST, BR_BLOCK_RISE, BR_F_HI, BR_F_LO, BR_GATE_BLOCK, BR_ATT_MODE, BR_F_ATT, BR_F_REL, BR_BAND_HYST, BR_HOLD_LATCH, BR_REL_REF, BR_PITCH_LP, BR_ATT_MIN, BR_ATT_SRC, BR_LOCK, BR_LOCK_THR, BR_SENS_IN, BR_FAST_IN, BR_REL_RESET, BR_RR_BAND, BR_RR_LO1, BR_RR_HI1, BR_RR_LO2, BR_RR_HI2, BR_RR_DEPTH, BR_NPARAMS
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

/* the learned lock (lock 1): a small network on the detector's state, evaluated every millisecond of a note's attack;
   the gain holds (until the next note) once its output passes the threshold. Lock 2 holds at given moments instead
   (an oracle, for checking the labels), and a dump collects the inputs of every attack millisecond for training. */
#define LK_IN 19
#define LK_MAXH 64
static double lk_w1[LK_MAXH * LK_IN], lk_b1[LK_MAXH], lk_w2[LK_MAXH], lk_b2 = 0.0; static int lk_h = 0;
__declspec(dllexport) int br_lock_inputs (void) { return LK_IN; }
__declspec(dllexport) void br_set_lock_net (int h, const double* w1, const double* b1, const double* w2, double b2)
{
    lk_h = h < LK_MAXH ? h : LK_MAXH;
    for (int j = 0; j < lk_h * LK_IN; ++j) lk_w1[j] = w1[j];
    for (int j = 0; j < lk_h; ++j) { lk_b1[j] = b1[j]; lk_w2[j] = w2[j]; }
    lk_b2 = b2;
}
static double lk_eval (const double* f)
{
    double z = lk_b2;
    for (int j = 0; j < lk_h; ++j)
    {
        double a = lk_b1[j];
        for (int q = 0; q < LK_IN; ++q) a += lk_w1[j * LK_IN + q] * f[q];
        z += lk_w2[j] * tanh (a);
    }
    return z;
}
/* (the oracle's schedule: pairs of a start's sample and its lock's sample, -1 for never; other starts use the rule) */
static const int* lk_sched = 0; static int lk_nsched = 0;
__declspec(dllexport) void br_set_lock_schedule (const int* t, int npairs) { lk_sched = t; lk_nsched = npairs; }
static double* lk_dump = 0; static int lk_dmax = 0, lk_dn = 0;
__declspec(dllexport) void br_set_lock_dump (double* buf, int maxframes) { lk_dump = buf; lk_dmax = maxframes; lk_dn = 0; }
__declspec(dllexport) int br_lock_dumped (void) { return lk_dn; }

__declspec(dllexport) void br_run (const float* x, int n, double fs, const double* p, float* gain, float* trace)
{
    Biquad shelf, lp, hp;
    lowshelf (&shelf, fs, p[BR_SH_FC], p[BR_SH_G], p[BR_SH_Q]);
    lowpass (&lp, fs, p[BR_LP_FC], p[BR_LP_Q]);
    Biquad lpp1, lpp2; lowpass (&lpp1, fs, p[BR_PITCH_LP] > 0.0 ? p[BR_PITCH_LP] : 1000.0, 0.7071); lpp2 = lpp1;
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
    double P = 0.0, Np = 0.0, Hp = 0.0, Rf = 0.0, Sl = 0.0, Fp = 0.0;
    const double cfa = coef (p[BR_F_ATT], fs), cfr = coef (p[BR_F_REL], fs);
    /* the windowed note level (N mode 1): sliding maxima of the low-passed waveform's square over 1024 / 2048 / 4096
       samples (at 48 kHz), the window picked by the pitch (the smallest spanning pitch_c periods) */
    double* qv = (double*) malloc (sizeof (double) * 3 * SMAX_CAP); int* qt = (int*) malloc (sizeof (int) * 3 * SMAX_CAP);
    SMax qs[3]; for (int k = 0; k < 3; ++k) smax_init (&qs[k], qv + k * SMAX_CAP, qt + k * SMAX_CAP, SMAX_CAP);
    const int wlen[3] = { (int) (1024 * fs / 48000.0 + 0.5), (int) (2048 * fs / 48000.0 + 0.5), (int) (4096 * fs / 48000.0 + 0.5) };
    double prevXp = 0.0, prevXl = 0.0, period = 0.01 * fs, cyclePos = 0.0, cycleNeg = 0.0, lastN = 0.0; int lastCross = -1, wsel = 0;
    /* the block level (block rise 1): the highest positive peak of the low-passed input in each block of a quarter
       of the window, held from the block's end; a rise is read on it */
    double blockMax = 0.0, Nb = -300.0; int blockPos = 0;
    int inNote = 0, released = 0, relCount = 0, below = 0, settle = 0, under = 0, rrCount = 0;
    double peakN = -300.0, g = 0.0, minN = 300.0, Nh = 300.0;
    /* the learned lock's state: 1 ms histories of E and N, the note's start and peaks */
    const int lkStep = (int) (fs / 1000.0 + 0.5); double lkE[64] = { 0 }, lkN[64] = { 0 }; int lkPos = 0, lkSince = 0;
    int locked = 0, startAt = -1, schedPtr = 0, schedLock = -2, lkFollow = 1, lkHeld = 0; double peakE = -300.0, Estart = -300.0, Ebefore = -300.0;
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
            /* a rising zero crossing: the period since the last one, if both halves of the cycle reached within 12 dB
               of the note level (not a filter's ringing in silence, nor a note's last part cycle) */
            if (xl > 0.0) cyclePos = fmax (cyclePos, xl * xl); else cycleNeg = fmax (cycleNeg, xl * xl);
            /* (pitch lp f: the crossings are read on the input low-passed again at f, 4 poles, so a strong harmonic
               does not cross; 0: on the note band itself) */
            const double xp = p[BR_PITCH_LP] > 0.0 ? bq (&lpp2, bq (&lpp1, xl)) : xl;
            const int crossed = prevXp <= 0.0 && xp > 0.0;
            prevXp = xp;
            if (crossed)
            {
                if (lastCross >= 0 && fmin (cyclePos, cycleNeg) > 0.0631 * lastN) { const double pd = (double) (i - lastCross); if (pd > 0.0004 * fs) period = pd; }
                lastCross = i; cyclePos = cycleNeg = 0.0;
            }
            prevXl = xl;
            const double want = p[BR_PITCH_C] * period;
            /* the window follows the pitch between notes and in a note's attack, then stays for the note (latch 1) */
            if (p[BR_WIN_LATCH] < 0.5 || ! inNote || under < settleN)
            {
                if (p[BR_F_HI] > 0.0)       /* the windows by pitch band: over f_hi 1024, over f_lo 2048, else 4096 */
                {
                    /* (band hyst h: a band is left only once the pitch is past its edge by the factor 1 + h) */
                    const double fz = fs / period, h = p[BR_BAND_HYST];
                    const double fu = fz / (1.0 + h), fd = fz / (1.0 - h);
                    const int up = fu > p[BR_F_HI] ? 0 : (fu > p[BR_F_LO] ? 1 : 2), dn = fd > p[BR_F_HI] ? 0 : (fd > p[BR_F_LO] ? 1 : 2);
                    if (up < wsel) wsel = up; else if (dn > wsel) wsel = dn;
                }
                else wsel = want <= wlen[0] ? 0 : (want <= wlen[1] ? 1 : 2);
            }
            double m[3];
            for (int k = 0; k < 3; ++k) m[k] = smax_push (&qs[k], i, xl * xl, wlen[k]);
            lastN = m[wsel];
            N = db_pow (m[wsel]);
            if (xl > blockMax) blockMax = xl;
            if (++blockPos >= wlen[wsel] / 4) { Nb = 20.0 * log10 (blockMax + 1e-15); blockMax = 0.0; blockPos = 0; }
        }
        else N = db_pow (follow (&Np, dp, cna, cnr));
        const double H = db_pow (follow (&Hp, quad_pow (&qh, bq (&hp, xi)), cna, cnr));
        /* the long reference and the slow envelope: peak followers of the quadrature power (env mode 0) or of the
           waveform's own peaks (env mode 1: no onset overshoot; BR_SLOW_REL_FAST) */
        const double pk = p[BR_NMODE] >= 1.0 ? pow (10.0, N / 10.0) : (p[BR_SLOW_REL_FAST] >= 1.0 ? xl * xl : dp);
        const double R = db_pow (peak (&Rf, pk, cra, krr));
        const double S = db_pow (peak (&Sl, pk, csa, ksr));
        /* the attack's reference (att mode 1): a one-pole follower of the note level's power (it lags a decaying note
           by a bounded amount, so a slow fade is followed throughout and a faster decay is held once it is the
           margin under it) */
        /* (att src 1: the follower and the test on the level read E instead of the note level N) */
        const double Fa = db_pow (follow (&Fp, p[BR_ATT_SRC] >= 0.5 ? P : pk, cfa, cfr));
        const double Na = p[BR_ATT_SRC] >= 0.5 ? E : N;
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
            else
            {
                /* (rr band 1: a release at a pitch in either band re-rides at once, from here; elsewhere it waits for a rise;
                   2: so at Response Fast only; 3: and once the gain holds; 4: as 3, but at its own depth under S, rr_depth,
                   whatever the release depth, released or not, within the gates; 5: as 4, at the release depth where that is deeper) */
                const double fz = fs / period;
                const int inBand = (p[BR_RR_BAND] >= 1.5 ? p[BR_FAST_IN] >= 0.5 : p[BR_RR_BAND] >= 0.5) && (p[BR_RR_BAND] < 2.5 || under >= settleN)
                                   && ((fz >= p[BR_RR_LO1] && fz <= p[BR_RR_HI1]) || (fz >= p[BR_RR_LO2] && fz <= p[BR_RR_HI2]));
                if (p[BR_RR_BAND] >= 3.5)
                {
                    rrCount = (inBand && open && N < S - (p[BR_RR_BAND] >= 4.5 ? fmax (p[BR_RR_DEPTH], p[BR_REL_DEPTH]) : p[BR_RR_DEPTH])) ? rrCount + 1 : 0;
                    if (rrCount >= relN) { start = 1; Sl = pow (10.0, N / 10.0); rrCount = 0; }
                }
                if (! released)
                {
                    /* released: under the slow envelope by the depth for long enough */
                    /* (rel ref 1: under the level where the gain was held, by the depth; S until it holds) */
                    const double Sr = (p[BR_REL_REF] >= 0.5 && under >= settleN) ? Nh : S;
                    relCount = N < Sr - p[BR_REL_DEPTH] ? relCount + 1 : 0;
                    if (relCount >= relN) { released = 1; minN = p[BR_BLOCK_RISE] >= 0.5 ? Nb : N; if (p[BR_FAST] >= 0.5 || (inBand && p[BR_RR_BAND] < 3.5)) { start = 1; Sl = pow (10.0, N / 10.0); } }   /* Fast: a release re-rides at once, from here */
                    /* or still sounding: a rising crossing of the slow envelope by the threshold */
                    if (N < S + p[BR_THR_U] - p[BR_HYST]) below = 1;
                    if (! released && below && open && N > S + p[BR_THR_U]) start = 1;
                }
                else
                {
                    /* released: a rise from the lowest since is a new note (within the gates); read on the block level
                       (block rise 1) or the note level */
                    const double Nr = p[BR_BLOCK_RISE] >= 0.5 ? Nb : N;
                    if (Nr < minN) minN = Nr;
                    const int openR = (p[BR_BLOCK_RISE] >= 0.5 && p[BR_GATE_BLOCK] >= 0.5) ? (Nr > p[BR_FLOOR] && legal && Nr > R - p[BR_ART]) : open;
                    if (openR && Nr > minN + p[BR_ON_RISE]) start = 1;
                }
            }
        }
        if (++lkSince >= lkStep) { lkSince = 0; lkPos = (lkPos + 1) & 63; lkE[lkPos] = E; lkN[lkPos] = N; }
        if (start)
        {
            /* (rel reset 1: a released note ridden afresh starts its slow envelope from here, so the next release needs a
               new drop by the depth) */
            if (p[BR_REL_RESET] >= 0.5 && inNote && released) Sl = pow (10.0, N / 10.0);
            inNote = 1; released = 0; relCount = 0; rrCount = 0; below = 0; peakN = N; g = gt; settle = settleN; under = 0; offCount = 0;
            locked = 0; lkFollow = 1; lkHeld = 0; startAt = i; peakE = E; Estart = E; Ebefore = lkE[(lkPos - 10) & 63];
            while (schedPtr < lk_nsched && lk_sched[2 * schedPtr] < i) ++schedPtr;
            schedLock = (schedPtr < lk_nsched && lk_sched[2 * schedPtr] == i) ? lk_sched[2 * schedPtr + 1] : -2;
        }
        else if (inNote)
        {
            /* the attack (the note over its slow envelope, which is still rising): the gain follows the law; then it
               only goes down */
            /* (hold latch 1: once held, the gain stays held until a new note) */
            if (! (p[BR_HOLD_LATCH] >= 0.5 && under >= settleN))
                under = Na > (p[BR_ATT_MODE] >= 0.5 ? Fa : S) + p[BR_THR_A] ? 0 : under + 1;
            if (under == settleN) Nh = N;                /* the level where the hold began */
            /* (att min 1: in the attack the gain only follows the law down, to its lowest) */
            if (E > peakE) peakE = E;
            const int since = i - startAt;
            /* the rule's own test, before its decision (an input of the learned one) */
            const int ruleUnder = under;
            if ((since % lkStep) == 0 && since / lkStep <= (p[BR_LOCK] >= 2.5 ? 1000 : 400) && (lk_dump || p[BR_LOCK] >= 0.5))
            {
                /* the lock's inputs, every millisecond of the attack */
                const int k = since / lkStep; double f[LK_IN];
                f[0] = k / 100.0; f[1] = log1p ((double) k) / 5.0; f[2] = (N - peakN) / 6.0; f[3] = (E - peakE) / 6.0;
                f[4] = (E - Estart) / 10.0; f[5] = (N - Fa) / 6.0; f[6] = (N - S) / 6.0; f[7] = (E - lkE[(lkPos - 5) & 63]) / 3.0;
                f[8] = (E - lkE[(lkPos - 20) & 63]) / 6.0; f[9] = (N - lkN[(lkPos - 10) & 63]) / 6.0; f[10] = p[BR_SENS_IN] / 100.0;
                f[11] = p[BR_FAST_IN]; f[12] = wsel / 2.0; f[13] = (N - R) / 10.0; f[14] = (peakE - Ebefore) / 20.0; f[15] = (N + 25.0) / 15.0;
                f[16] = ruleUnder < settleN ? 1.0 : 0.0; f[17] = fmin (ruleUnder, 4 * settleN) / (double) settleN / 4.0;
                f[18] = (Na - (p[BR_ATT_MODE] >= 0.5 ? Fa : S) - p[BR_THR_A]) / 6.0;
                if (lk_dump && lk_dn < lk_dmax) { double* d = lk_dump + (LK_IN + 2) * lk_dn++; d[0] = i; d[1] = k; for (int q = 0; q < LK_IN; ++q) d[2 + q] = f[q]; }
                if (p[BR_LOCK] >= 0.5 && p[BR_LOCK] < 1.5 && ! locked && lk_h > 0 && lk_eval (f) > p[BR_LOCK_THR]) locked = 1;
                if (p[BR_LOCK] >= 2.5 && lk_h > 0)
                {
                    /* (lock 3: follow or hold, every ms; once it has held, it follows again only where the rule does too) */
                    const int want = lk_eval (f) > p[BR_LOCK_THR];
                    if (! want) lkHeld = 1;
                    lkFollow = want && (! lkHeld || p[BR_LOCK] >= 3.5 || ruleUnder < settleN);
                }
            }
            if (p[BR_LOCK] >= 1.5 && schedLock != -2)
            {
                if (! locked && schedLock >= 0 && i >= schedLock) locked = 1;
                if (! locked) g = gt;
            }
            else if (p[BR_LOCK] >= 0.5 && p[BR_LOCK] < 1.5) { if (! locked) g = gt; }
            else if (p[BR_LOCK] >= 2.5)
            {
                if (since / lkStep > 1000) lkFollow = ruleUnder < settleN;   /* (after a second: the rule) */
                if (lkFollow) g = gt;
            }
            else if (under < settleN) g = (p[BR_ATT_MIN] >= 0.5 && gt > g) ? g : gt;   /* (BR_SETTLE: how long under it before the hold, ms) */
            else if (p[BR_HOLD_MIN] >= 0.5 && gt < g) g = gt;   /* (hold_min 1: the hold only goes down; 0: constant) */
        }
        if (inNote)
            raw = fmin (p[BR_RMAX], fmax (p[BR_RMIN], g));
        else if (N < peakN - p[BR_OFF])     /* silence (38 dB under the last note): the glide to the Idle value */
            raw = raw > p[BR_IDLE] ? fmax (p[BR_IDLE], raw - glide) : fmin (p[BR_IDLE], raw + glide);
        /* (an ignored note, within 38 dB of the last: the gain stays where it is) */
        out = raw + csm * (out - raw);
        gain[i] = (float) out;
        if (trace) { float* t = trace + 6 * i; t[0] = p[BR_NMODE] >= 2.0 ? (float) (wsel * 100000 + period) : (float) E; t[1] = (float) N; t[2] = (float) (inNote + 2 * released + 4 * below + 8 * start); t[3] = p[BR_ATT_MODE] >= 0.5 ? (float) Fa : (float) raw; t[4] = (float) S; t[5] = (float) R; }
    }
    free (qv); free (qt);
}
