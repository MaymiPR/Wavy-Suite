/* WComp Phase 4: H-Comp's release, fifth form (loaded by ident_hc_rel4.py with FORM = 5): two averages of R.

   Rt: the target gain reduction per sample (dB, >= 0) from the look-ahead hold and the static curve.
   S:  a slow state. avg = 0: charges toward the output R (a one-pole on the linear gain, time constant crest) and
       otherwise releases at Fs(S) / release + As(S) dB/s, not below R. avg = 1: the plain average of R (the same
       one-pole both ways, the reference's "Crest" detector). At start-up it holds S0 (a fresh plug-in releases slowly
       until S has come down).
   R:  the output: rises to the target at once, falls at Fs(R) / (release * w(R - S)) + As(R) dB/s, never below the
       target; w = 1 while R <= S and shrinks as R rises above S (short events release almost at once).
   Fs, As, w: log-value tables on knots.
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

__declspec(dllexport) void hc5(const double* Rt, double* g, double* Rout, double* Sout, int n, double fs, double rel_ms,
                               double att_c, int B1, int B2,
                               int nF, const double* Fx, const double* Fv, int nA, const double* Ax, const double* Av,
                               int nW, const double* Wx, const double* Wv, double crest_ms, double S0, int avg,
                               int nW2, const double* W2x, const double* W2v, double fast_ms, double S20)
{
    const double dt = 1.0 / fs, rel = rel_ms * 1e-3, cc = exp(-1.0 / (crest_ms * 1e-3 * fs)), cf = exp(-1.0 / (fast_ms * 1e-3 * fs));
    double R = 0.0, S = S0, S2 = S20, g1 = 1.0;
    double* s1 = (double*) malloc(sizeof(double) * B1);
    double* s2 = (double*) malloc(sizeof(double) * B2);
    for (int k = 0; k < B1; ++k) s1[k] = 1.0;
    for (int k = 0; k < B2; ++k) s2[k] = 1.0;
    double sum1 = B1, sum2 = B2;
    int i1 = 0, i2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = Rt[i];
        /* the output: the release time shrinks with the distance above the slow state */
        const double w = (R > S ? tab(nW, Wx, Wv, R - S) : exp(Wv[0])) * (R > S2 ? tab(nW2, W2x, W2v, R - S2) : exp(W2v[0]));
        R -= (tab(nF, Fx, Fv, R) / (rel * w) + tab(nA, Ax, Av, R)) * dt;
        if (R < t) R = t;
        if (R < 0.0) R = 0.0;
        /* the slow state */
        {
            const double gs = pow(10.0, -S2 / 20.0), gt = pow(10.0, -R / 20.0);
            S2 = -20.0 * log10(gt + cf * (gs - gt));
        }
        if (avg)
        {
            const double gs = pow(10.0, -S / 20.0), gt = pow(10.0, -R / 20.0);
            S = -20.0 * log10(gt + cc * (gs - gt));
        }
        else if (R > S)
        {
            const double gs = pow(10.0, -S / 20.0), gt = pow(10.0, -R / 20.0);
            S = -20.0 * log10(gt + cc * (gs - gt));
            if (S > R) S = R;
        }
        else
        {
            S -= (tab(nF, Fx, Fv, S) / rel + tab(nA, Ax, Av, S)) * dt;
            if (S < R) S = R;
        }
        const double gt = pow(10.0, -R / 20.0);
        g1 = gt < g1 ? gt + att_c * (g1 - gt) : gt;
        sum1 += g1 - s1[i1]; s1[i1] = g1; i1 = (i1 + 1) % B1;
        const double v1 = sum1 / B1;
        sum2 += v1 - s2[i2]; s2[i2] = v1; i2 = (i2 + 1) % B2;
        g[i] = sum2 / B2;
        if (Rout) Rout[i] = R;
        if (Sout) Sout[i] = S;
    }
    free(s1); free(s2);
}
