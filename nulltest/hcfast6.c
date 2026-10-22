/* WComp Phase 4: H-Comp's release, sixth form (loaded by ident_hc_rel6.py): the fourth form with a learned
   correction of its release-time factor.

   R falls at Fs(R) / (release * w) + As(R) dB/s, never below the target Rt, with
       ln w = ln w1(R - S) + m(features),   m <= 0 allowed to either side, a small MLP (8 tanh units),
   features: R / 20, (R - S) / 10, (R - S2) / 10, (R - S3) / 10, ln(release / 100 ms) / 2, where S, S2, S3 are averages
   of R (one-pole on the linear gain) with time constants crest (the slow one), 30 ms and 5 ms; all start at S0 after a
   reset (a fresh plug-in releases slowly at first).
   g = box(B1) * box(B2) of the one-pole (attack, falling only) of 10^(-R/20).                                    */
#include <math.h>
#include <stdlib.h>

static double tab(int n, const double* x, const double* v, double t)
{
    if (t <= x[0]) return exp(v[0]);
    if (t >= x[n - 1]) return exp(v[n - 1]);
    int i = 0;
    while (t > x[i + 1]) ++i;
    const double f = (t - x[i]) / (x[i + 1] - x[i]);
    return exp(v[i] + f * (v[i + 1] - v[i]));
}

#define NIN 5
#define NH 8

/* mlp: W1[NH][NIN], b1[NH], W2[NH], b2 */
static double mlp(const double* p, const double* in)
{
    double o = p[NH * NIN + NH + NH];
    for (int h = 0; h < NH; ++h)
    {
        double a = p[NH * NIN + h];
        for (int k = 0; k < NIN; ++k) a += p[h * NIN + k] * in[k];
        o += p[NH * NIN + NH + h] * tanh(a);
    }
    return o;
}

static double avg(double S, double R, double c)
{
    const double gs = pow(10.0, -S / 20.0), gt = pow(10.0, -R / 20.0);
    return -20.0 * log10(gt + c * (gs - gt));
}

__declspec(dllexport) void hc6(const double* Rt, double* g, double* Rout, int n, double fs, double rel_ms, double att_c, int B1, int B2,
                               int nF, const double* Fx, const double* Fv, int nA, const double* Ax, const double* Av,
                               int nW, const double* Wx, const double* Wv, double crest_ms, double S0, const double* net)
{
    const double dt = 1.0 / fs, rel = rel_ms * 1e-3, lr = log(rel_ms / 100.0) / 2.0;
    const double c1 = exp(-1.0 / (crest_ms * 1e-3 * fs)), c2 = exp(-1.0 / (30e-3 * fs)), c3 = exp(-1.0 / (5e-3 * fs));
    double R = 0.0, S = S0, S2 = S0, S3 = S0, g1 = 1.0;
    double* s1 = (double*) malloc(sizeof(double) * B1);
    double* s2 = (double*) malloc(sizeof(double) * B2);
    for (int k = 0; k < B1; ++k) s1[k] = 1.0;
    for (int k = 0; k < B2; ++k) s2[k] = 1.0;
    double sum1 = B1, sum2 = B2;
    int i1 = 0, i2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = Rt[i];
        if (R > t)
        {
            const double in[NIN] = { R / 20.0, (R - S) / 10.0, (R - S2) / 10.0, (R - S3) / 10.0, lr };
            const double lw = log(R > S ? tab(nW, Wx, Wv, R - S) : exp(Wv[0])) + mlp(net, in);
            R -= (tab(nF, Fx, Fv, R) / (rel * exp(lw)) + tab(nA, Ax, Av, R)) * dt;
        }
        if (R < t) R = t;
        if (R < 0.0) R = 0.0;
        S = avg(S, R, c1); S2 = avg(S2, R, c2); S3 = avg(S3, R, c3);
        const double gt = pow(10.0, -R / 20.0);
        g1 = gt < g1 ? gt + att_c * (g1 - gt) : gt;
        sum1 += g1 - s1[i1]; s1[i1] = g1; i1 = (i1 + 1) % B1;
        const double v1 = sum1 / B1;
        sum2 += v1 - s2[i2]; s2[i2] = v1; i2 = (i2 + 1) % B2;
        g[i] = sum2 / B2;
        if (Rout) Rout[i] = R;
    }
    free(s1); free(s2);
}
