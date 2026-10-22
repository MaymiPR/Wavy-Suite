/* WComp Phase 4: the release law of train_hc_release.py (hcfast6.c with a learned tau1) and its exact gradient
   (reverse-mode, written out by hand), one case per call; loaded by train_hc_release2.py.

   Parameters p (natural-log tables): logF[nF] on Fx, logA[nA] on Ax, logW[nW] on Wx, logTau, S0, then the MLP
   W1[NH][5], b1[NH], W2[NH], b2.  Per sample, from the state R, G1..G3 (linear averages of 10^(-R/20)), g1:
     S_k = -20 log10 G_k, d1 = R - S1, w1 = d1 > 0 ? W(d1) : exp(logW[0])
     m = W2 . tanh(W1 f + b1) + b2,  f = (R/20, d1/10, (R-S2)/10, (R-S3)/10, ln(rel/100)/2)
     rate = F(R) / (rel w1 e^m) + A(R);  R' = max(max(R - rate/fs, 0), t)
     gR = 10^(-R'/20);  G_k' = gR + c_k (G_k - gR) (c1 from tau1, c2: 30 ms, c3: 5 ms);  g1' = attack one-pole of gR
   y = box27(box38(g1')); loss = sum over masked samples (every dec-th) of (20 log10 y - Gdb)^2.             */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NIN 5
#define NH 8
#define LN10_20 0.11512925464970229   /* ln(10) / 20 */

static double KSM = 0.0;      /* gate softness in dB (0: hard, as the reference) */
__declspec(dllexport) void hc_set_soft(double k) { KSM = k; }
static double softplus(double z) { return z > 30.0 ? z : log1p(exp(z)); }
static double sigm(double z) { return 1.0 / (1.0 + exp(-z)); }

typedef struct { int i; double f, v, slope; } Look;     /* value exp(.) and d value / dx */

static Look look(int n, const double* x, const double* lv, double t)
{
    Look L;
    if (t <= x[0]) { L.i = 0; L.f = 0.0; L.v = exp(lv[0]); L.slope = 0.0; return L; }
    if (t >= x[n - 1]) { L.i = n - 2; L.f = 1.0; L.v = exp(lv[n - 1]); L.slope = 0.0; return L; }
    int i = 0;
    while (t > x[i + 1]) ++i;
    L.i = i; L.f = (t - x[i]) / (x[i + 1] - x[i]);
    L.v = exp(lv[i] + L.f * (lv[i + 1] - lv[i]));
    L.slope = L.v * (lv[i + 1] - lv[i]) / (x[i + 1] - x[i]);
    return L;
}

static void look_grad(const Look* L, double a, double* g)    /* a = dLoss / d value */
{
    g[L->i] += a * L->v * (1.0 - L->f);
    g[L->i + 1] += a * L->v * L->f;
}

typedef struct
{
    int nF, nA, nW;
    const double *Fx, *Ax, *Wx;
} Knots;

/* one step forward; fills the intermediates the reverse pass needs */
typedef struct
{
    double S[3], d1, w1, f[NIN], h[NH], m, q, F, A, rate, Rp0, Rp, Rn, gR;
    Look LF, LA, LW;
    int wpos, zpos, upos, fall;
    double sz, su, sf;          /* the gates' derivatives (soft gates) or 0 / 1 (hard) */
} Step;

static void step(const Knots* K, const double* p, double rel, double lr, const double* c, double att_c,
                 double R, const double* G, double g1, double t, Step* s, double* G_out, double* g1_out)
{
    const double *logF = p, *logA = p + K->nF, *logW = p + K->nF + K->nA;
    const double* net = p + K->nF + K->nA + K->nW + 2;
    for (int k = 0; k < 3; ++k) s->S[k] = -20.0 * log10(G[k]);
    s->d1 = R - s->S[0];
    s->wpos = s->d1 > 0.0;
    if (s->wpos) { s->LW = look(K->nW, K->Wx, logW, s->d1); s->w1 = s->LW.v; }
    else s->w1 = exp(logW[0]);
    s->f[0] = R / 20.0; s->f[1] = s->d1 / 10.0; s->f[2] = (R - s->S[1]) / 10.0; s->f[3] = (R - s->S[2]) / 10.0; s->f[4] = lr;
    const double *W1 = net, *b1 = net + NH * NIN, *W2 = net + NH * NIN + NH, b2 = net[NH * NIN + NH + NH];
    s->m = b2;
    for (int h = 0; h < NH; ++h)
    {
        double a = b1[h];
        for (int k = 0; k < NIN; ++k) a += W1[h * NIN + k] * s->f[k];
        s->h[h] = tanh(a);
        s->m += W2[h] * s->h[h];
    }
    s->LF = look(K->nF, K->Fx, logF, R); s->F = s->LF.v;
    s->LA = look(K->nA, K->Ax, logA, R); s->A = s->LA.v;
    s->q = rel * s->w1 * exp(s->m);
    s->rate = s->F / s->q + s->A;
    s->Rp0 = R - s->rate / 48000.0;
    if (KSM > 0.0)
    {
        /* soft gates for training: max(a, b) ~ b + k softplus((a - b) / k) */
        s->Rp = KSM * softplus(s->Rp0 / KSM); s->sz = sigm(s->Rp0 / KSM);
        s->Rn = t + KSM * softplus((s->Rp - t) / KSM); s->su = sigm((s->Rp - t) / KSM);
    }
    else
    {
        s->zpos = s->Rp0 > 0.0;  s->Rp = s->zpos ? s->Rp0 : 0.0; s->sz = s->zpos;
        s->upos = s->Rp >= t;     s->Rn = s->upos ? s->Rp : t;  s->su = s->upos;
    }
    s->gR = exp(-s->Rn * LN10_20);
    for (int k = 0; k < 3; ++k) G_out[k] = s->gR + c[k] * (G[k] - s->gR);
    if (KSM > 0.0)
    {
        const double k2 = KSM * 1e-2;   /* in linear gain */
        *g1_out = s->gR + att_c * k2 * softplus((g1 - s->gR) / k2); s->sf = sigm((g1 - s->gR) / k2);
    }
    else
    {
        s->fall = s->gR < g1; s->sf = s->fall;
        *g1_out = s->fall ? s->gR + att_c * (g1 - s->gR) : s->gR;
    }
}

/* returns the loss (sum) and adds its gradient to grad; count = number of loss samples */
__declspec(dllexport) double hc_grad(int nF, const double* Fx, int nA, const double* Ax, int nW, const double* Wx,
                                     const double* p, const double* Rt, const double* Gdb, const double* mask, int n,
                                     double rel_ms, double att_ms, int startup, int dec, double* grad, double* count,
                                     double* yout)
{
    Knots K = { nF, nA, nW, Fx, Ax, Wx };
    const double fs = 48000.0, rel = rel_ms * 1e-3, lr = log(rel_ms / 100.0) / 2.0;
    const double tau = exp(p[nF + nA + nW]), S0 = p[nF + nA + nW + 1];
    const double c[3] = { exp(-1.0 / (tau * 1e-3 * fs)), exp(-1.0 / (30e-3 * fs)), exp(-1.0 / (5e-3 * fs)) };
    const double att_c = exp(-1.0 / (att_ms * 0.99e-3 * fs));
    double* st = (double*) malloc(sizeof(double) * 5 * (size_t) (n + 1));   /* R, G1, G2, G3, g1 before each step */
    double* g1s = (double*) malloc(sizeof(double) * (size_t) n);
    double R = 0.0, G[3], g1 = 1.0;
    const double G0 = startup ? exp(-S0 * LN10_20) : 1.0;
    for (int k = 0; k < 3; ++k) G[k] = G0;
    Step s;
    for (int i = 0; i < n; ++i)
    {
        double* q = st + 5 * i; q[0] = R; q[1] = G[0]; q[2] = G[1]; q[3] = G[2]; q[4] = g1;
        double Gn[3], g1n;
        step(&K, p, rel, lr, c, att_c, R, G, g1, Rt[i], &s, Gn, &g1n);
        R = s.Rn; G[0] = Gn[0]; G[1] = Gn[1]; G[2] = Gn[2]; g1 = g1n;
        g1s[i] = g1;
    }
    /* the two moving averages and the loss */
    double* v1 = (double*) malloc(sizeof(double) * (size_t) n);
    double* y = (double*) malloc(sizeof(double) * (size_t) n);
    double* ay = (double*) calloc((size_t) n, sizeof(double));
    double* av = (double*) calloc((size_t) n, sizeof(double));
    double* ag = (double*) calloc((size_t) n, sizeof(double));
    { double sum = 38.0; for (int i = 0; i < n; ++i) { sum += g1s[i] - (i >= 38 ? g1s[i - 38] : 1.0); v1[i] = sum / 38.0; } }
    { double sum = 27.0; for (int i = 0; i < n; ++i) { sum += v1[i] - (i >= 27 ? v1[i - 27] : 1.0); y[i] = sum / 27.0; } }
    double loss = 0.0, cnt = 0.0;
    for (int i = 0; i < n; i += dec)
    {
        if (mask[i] <= 0.0) continue;
        const double yy = y[i] > 1e-6 ? y[i] : 1e-6;
        const double e = 20.0 * log10(yy) - Gdb[i];
        loss += e * e; cnt += 1.0;
        ay[i] = y[i] > 1e-6 ? 2.0 * e * 20.0 / (log(10.0) * yy) : 0.0;
    }
    if (yout) memcpy(yout, y, sizeof(double) * (size_t) n);
    *count += cnt;
    /* reverse through the moving averages: d/dx[i] = (1/n) sum_{j = i .. i+n-1} d/dy[j] */
    { double sum = 0.0; for (int i = n - 1; i >= 0; --i) { sum += ay[i] - (i + 27 < n ? ay[i + 27] : 0.0); av[i] = sum / 27.0; } }
    { double sum = 0.0; for (int i = n - 1; i >= 0; --i) { sum += av[i] - (i + 38 < n ? av[i + 38] : 0.0); ag[i] = sum / 38.0; } }
    /* reverse through the steps */
    double *gF = grad, *gA = grad + nF, *gW = grad + nF + nA, *gNet = grad + nF + nA + nW + 2;
    const double* net = p + nF + nA + nW + 2;
    const double *W1 = net, *W2 = net + NH * NIN + NH;
    double aR = 0.0, aG[3] = { 0, 0, 0 }, ag1 = 0.0, ac1 = 0.0;
    for (int i = n - 1; i >= 0; --i)
    {
        const double* q = st + 5 * i;
        const double Rp = q[0], Gp[3] = { q[1], q[2], q[3] }, g1p = q[4];
        double Gn[3], g1n;
        step(&K, p, rel, lr, c, att_c, Rp, Gp, g1p, Rt[i], &s, Gn, &g1n);
        const double A1 = ag1 + ag[i];
        double a_gR = 0.0, a_g1p;
        a_gR += (1.0 - att_c * s.sf) * A1; a_g1p = att_c * s.sf * A1;
        double a_Gp[3];
        for (int k = 0; k < 3; ++k) { a_gR += (1.0 - c[k]) * aG[k]; a_Gp[k] = c[k] * aG[k]; }
        ac1 += (Gp[0] - s.gR) * aG[0];
        const double a_Rn = aR - a_gR * s.gR * LN10_20;
        const double a_Rp = s.su * a_Rn;
        const double a_Rp0 = s.sz * a_Rp;
        double a_R = a_Rp0;
        double a_S[3] = { 0, 0, 0 };
        if (a_Rp0 != 0.0)          /* R did not end at the target or at 0: the release law took part */
        {
            const double a_rate = -a_Rp0 / fs;
            const double a_F = a_rate / s.q, a_q = -a_rate * s.F / (s.q * s.q), a_A = a_rate;
            a_R += a_F * s.LF.slope + a_A * s.LA.slope;
            look_grad(&s.LF, a_F, gF); look_grad(&s.LA, a_A, gA);
            const double a_w1 = a_q * s.q / s.w1, a_m = a_q * s.q;
            double a_f[NIN] = { 0, 0, 0, 0, 0 };
            gNet[NH * NIN + NH + NH] += a_m;
            for (int h = 0; h < NH; ++h)
            {
                gNet[NH * NIN + NH + h] += a_m * s.h[h];
                const double a_pre = a_m * W2[h] * (1.0 - s.h[h] * s.h[h]);
                gNet[NH * NIN + h] += a_pre;
                for (int k = 0; k < NIN; ++k) { gNet[h * NIN + k] += a_pre * s.f[k]; a_f[k] += a_pre * W1[h * NIN + k]; }
            }
            a_R += a_f[0] / 20.0 + a_f[1] / 10.0 + a_f[2] / 10.0 + a_f[3] / 10.0;
            a_S[0] -= a_f[1] / 10.0; a_S[1] -= a_f[2] / 10.0; a_S[2] -= a_f[3] / 10.0;
            if (s.wpos)
            {
                const double a_d1 = a_w1 * s.LW.slope;
                look_grad(&s.LW, a_w1, gW);
                a_R += a_d1; a_S[0] -= a_d1;
            }
            else gW[0] += a_w1 * s.w1;
        }
        for (int k = 0; k < 3; ++k) a_Gp[k] += a_S[k] * (-20.0 / (log(10.0) * Gp[k]));
        aR = a_R; ag1 = a_g1p;
        for (int k = 0; k < 3; ++k) aG[k] = a_Gp[k];
    }
    /* tau1 (through c1) and S0 (through the start) */
    grad[nF + nA + nW] += ac1 * c[0] / (tau * 1e-3 * fs);
    if (startup) grad[nF + nA + nW + 1] += (aG[0] + aG[1] + aG[2]) * (-LN10_20 * G0);
    free(st); free(g1s); free(v1); free(y); free(ay); free(av); free(ag);
    return loss;
}
