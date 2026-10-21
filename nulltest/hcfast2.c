/* WComp Phase 4: H-Comp's release as a two-state law in the gain-reduction domain (loaded by ident_hc_release2.py).

   Rt: the target gain reduction per sample (dB, >= 0) from the look-ahead hold and the static curve.
   S:  a slow state. Above the target it releases at Fs(S)/release + As(S) dB/s; below it charges at C(S) dB/s
       (never past the target).
   R:  the output: rises to the target at once, falls toward S at Ff(R - S) dB/s, never below S or the target.
   The laws are tables on knots (linear interpolation; the values are exponentiated, so the rates stay positive).
   g = box(B1) * box(B2) of the one-pole (attack, falling only) of 10^(-R/20).                                    */
#include <math.h>
#include <stdlib.h>

typedef struct { int n; const double* x; const double* v; } Tab;

static double tab(const Tab* t, double x)
{
    if (x <= t->x[0]) return exp(t->v[0]);
    if (x >= t->x[t->n - 1]) return exp(t->v[t->n - 1]);
    int i = 0;
    while (x > t->x[i + 1]) ++i;
    const double f = (x - t->x[i]) / (t->x[i + 1] - t->x[i]);
    return exp(t->v[i] + f * (t->v[i + 1] - t->v[i]));
}

/* the effective release table: log ms on a grid of R (rows) and S (columns), set by hc_set_rel2d */
static int nR2 = 0, nS2 = 0, attackFirst = 0, useMlp = 0, chargeFromR = 0, useW0 = 0;
static double W0x[16], W0v[16], Kms = 56.23, Kp = 1.42; static int nW0 = 0;
/* rel_eff = min(release, release * w0(S), K / (R - S)^p); w0 a log table over S */
__declspec(dllexport) void hc_set_w0(int n, const double* xs, const double* v, double K, double p)
{ useW0 = 1; nW0 = n; for (int k = 0; k < n; ++k) { W0x[k] = xs[k]; W0v[k] = v[k]; } Kms = K; Kp = p; }
static double w0_of(double S)
{
    if (S <= W0x[0]) return exp(W0v[0]);
    if (S >= W0x[nW0 - 1]) return exp(W0v[nW0 - 1]);
    int i = 0; while (S > W0x[i + 1]) ++i;
    const double f = (S - W0x[i]) / (W0x[i + 1] - W0x[i]);
    return exp(W0v[i] + f * (W0v[i + 1] - W0v[i]));
}
__declspec(dllexport) void hc_set_charge_from_r(int a) { chargeFromR = a; }
/* an MLP for the effective release: inputs R/30, S/30, (R-S)/30, ln(release / 100 ms)/3; 8 tanh units; output w,
   rel_eff = release * sigmoid(w)                                                                           */
static double W1[8 * 4], B1m[8], W2[8], B2m;
__declspec(dllexport) void hc_set_mlp(const double* p) { useMlp = 1; for (int k = 0; k < 32; ++k) W1[k] = p[k]; for (int k = 0; k < 8; ++k) { B1m[k] = p[32 + k]; W2[k] = p[40 + k]; } B2m = p[48]; }
static double mlp_w(double R, double S, double lr)
{
    const double in[4] = { R / 30.0, S / 30.0, (R - S) / 30.0, lr };
    double o = B2m;
    for (int h = 0; h < 8; ++h)
    {
        double a = B1m[h];
        for (int k = 0; k < 4; ++k) a += W1[h * 4 + k] * in[k];
        o += W2[h] * tanh(a);
    }
    return 1.0 / (1.0 + exp(-o));
}
__declspec(dllexport) void hc_set_order(int a) { attackFirst = a; }
static double R2[32], S2[32], V2[32 * 32];
__declspec(dllexport) void hc_set_rel2d(int nr, const double* r, int ns, const double* sv, const double* v)
{
    nR2 = nr; nS2 = ns;
    for (int i = 0; i < nr; ++i) R2[i] = r[i];
    for (int j = 0; j < ns; ++j) S2[j] = sv[j];
    for (int k = 0; k < nr * ns; ++k) V2[k] = v[k];
}
static double rel2d(double R, double S)
{
    int i = 0, j = 0;
    if (R <= R2[0]) R = R2[0];
    if (R >= R2[nR2 - 1]) R = R2[nR2 - 1];
    if (S <= S2[0]) S = S2[0];
    if (S >= S2[nS2 - 1]) S = S2[nS2 - 1];
    while (i < nR2 - 2 && R > R2[i + 1]) ++i;
    while (j < nS2 - 2 && S > S2[j + 1]) ++j;
    const double fr = (R - R2[i]) / (R2[i + 1] - R2[i]), fsv = (S - S2[j]) / (S2[j + 1] - S2[j]);
    const double v = (1 - fr) * (1 - fsv) * V2[i * nS2 + j] + fr * (1 - fsv) * V2[(i + 1) * nS2 + j]
                   + (1 - fr) * fsv * V2[i * nS2 + j + 1] + fr * fsv * V2[(i + 1) * nS2 + j + 1];
    return exp(v);
}

/* knots: xs = [Fs knots | As knots | C knots | Ff knots], vs likewise; ns = counts of each */
__declspec(dllexport) void hc_release(const double* Rt, double* g, double* Rout, double* Sout, int n, double fs, double rel_ms,
                                      double att_c, int B1, int B2, const int* ns, const double* xs, const double* vs)
{
    Tab Fs = { ns[0], xs, vs }, As = { ns[1], xs + ns[0], vs + ns[0] };
    Tab C = { ns[2], xs + ns[0] + ns[1], vs + ns[0] + ns[1] }, Ff = { ns[3], xs + ns[0] + ns[1] + ns[2], vs + ns[0] + ns[1] + ns[2] };
    const double dt = 1.0 / fs, rel = rel_ms * 1e-3;
    double R = 0.0, S = 0.0, g1 = 1.0;               /* the plug-in starts uncompressed */
    double* s1 = (double*) malloc(sizeof(double) * B1);
    double* s2 = (double*) malloc(sizeof(double) * B2);
    for (int k = 0; k < B1; ++k) s1[k] = g1;
    for (int k = 0; k < B2; ++k) s2[k] = g1;
    double sum1 = g1 * B1, sum2 = g1 * B2;
    int i1 = 0, i2 = 0;
    double ga = 1.0;
    for (int i = 0; i < n; ++i)
    {
        double t = Rt[i];
        if (attackFirst)
        {
            /* the attack smooths the target (linear gain, falling only) before the release machinery */
            const double gt0 = pow(10.0, -t / 20.0);
            ga = gt0 < ga ? gt0 + att_c * (ga - gt0) : gt0;
            t = -20.0 * log10(ga);
        }
        const double src = chargeFromR ? (R > t ? R : t) : t;   /* what S charges toward: the target, or the output */
        if (src > S && (!chargeFromR || t > S || R > S))
        {
            /* charge: a one-pole on the linear gain toward the target, time constant Ff.v[2] ms ("Crest") */
            const double gs = pow(10.0, -S / 20.0), gt0 = pow(10.0, -src / 20.0), c = exp(-1.0 / (Ff.v[2] * 1e-3 * fs));
            S = -20.0 * log10(gt0 + c * (gs - gt0));
            if (S > src) S = src;
        }
        else { S -= (tab(&Fs, S) / rel + tab(&As, S)) * dt; if (S < t) S = t; }
        if (S < 0.0) S = 0.0;
        /* the output falls with the same law as S, at an effective release time that shrinks with its distance
           above S: rel_eff = min(release, K / (R - S)^p); Ff's first two knot values hold log K (ms) and p */
        {
            double re = rel;
            if (R - S > 1e-9)
            {
                double r2;
                if (useW0) { r2 = Kms * 1e-3 / pow(R - S, Kp); const double r3 = rel * w0_of(S); if (r3 < r2) r2 = r3; }
                else r2 = useMlp ? rel * mlp_w(R, S, log(rel_ms / 100.0) / 3.0)
                                 : rel2d (R, S) * 1e-3;  /* the effective release (ms) as a table over (R, S) */
                if (r2 < re) re = r2;
            }
            R -= (tab(&Fs, R) / re + tab(&As, R)) * dt;
        }
        if (R < S) R = S;
        if (R < t) R = t;
        const double gt = pow(10.0, -R / 20.0);
        g1 = attackFirst ? gt : (gt < g1 ? gt + att_c * (g1 - gt) : gt);
        sum1 += g1 - s1[i1]; s1[i1] = g1; i1 = (i1 + 1) % B1;
        const double v1 = sum1 / B1;
        sum2 += v1 - s2[i2]; s2[i2] = v1; i2 = (i2 + 1) % B2;
        g[i] = sum2 / B2;
        if (Rout) Rout[i] = R;
        if (Sout) Sout[i] = S;
    }
    free(s1); free(s2);
}
