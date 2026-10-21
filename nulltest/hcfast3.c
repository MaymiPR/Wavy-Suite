/* WComp Phase 4: H-Comp's release, third form (loaded by ident_hc_rel3.py).

   Rt: the target gain reduction per sample (dB, >= 0) from the look-ahead hold and the static curve.
   S:  a slow state: charges toward the output R (a one-pole on the linear gain, time constant crest), releases at
       Fs(S) / release + As(S) dB/s, never below the target.
   R:  the output: rises to the target at once, falls at Fs(R) / release + As(R) + Ff(R, S) dB/s, never below S or
       the target. Ff is the fast part (independent of the release time): large while R is deep and S has not
       caught up, as after short events.
   Fs, As: log-value tables on knots; Ff: log values on an (R, S) grid, bilinear.
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

static double grid(int nr, const double* rk, int ns, const double* sk, const double* v, double R, double S)
{
    if (R < rk[0]) R = rk[0];
    if (R > rk[nr - 1]) R = rk[nr - 1];
    if (S < sk[0]) S = sk[0];
    if (S > sk[ns - 1]) S = sk[ns - 1];
    int i = 0, j = 0;
    while (i < nr - 2 && R > rk[i + 1]) ++i;
    while (j < ns - 2 && S > sk[j + 1]) ++j;
    const double fr = (R - rk[i]) / (rk[i + 1] - rk[i]), fq = (S - sk[j]) / (sk[j + 1] - sk[j]);
    const double a = (1 - fr) * (1 - fq) * v[i * ns + j] + fr * (1 - fq) * v[(i + 1) * ns + j]
                   + (1 - fr) * fq * v[i * ns + j + 1] + fr * fq * v[(i + 1) * ns + j + 1];
    return exp(a);
}

__declspec(dllexport) void hc3(const double* Rt, double* g, double* Rout, double* Sout, int n, double fs, double rel_ms,
                               double att_c, int B1, int B2,
                               int nF, const double* Fx, const double* Fv, int nA, const double* Ax, const double* Av, double crest_ms,
                               int nR, const double* Rk, int nS, const double* Sk, const double* Ffv)
{
    const double dt = 1.0 / fs, rel = rel_ms * 1e-3, cc = exp(-1.0 / (crest_ms * 1e-3 * fs));
    double R = 0.0, S = 0.0, g1 = 1.0;
    double* s1 = (double*) malloc(sizeof(double) * B1);
    double* s2 = (double*) malloc(sizeof(double) * B2);
    for (int k = 0; k < B1; ++k) s1[k] = 1.0;
    for (int k = 0; k < B2; ++k) s2[k] = 1.0;
    double sum1 = B1, sum2 = B2;
    int i1 = 0, i2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = Rt[i];
        const double src = R > t ? R : t;
        if (src > S)
        {
            const double gs = pow(10.0, -S / 20.0), gt = pow(10.0, -src / 20.0);
            S = -20.0 * log10(gt + cc * (gs - gt));
            if (S > src) S = src;
        }
        else { S -= (tab(nF, Fx, Fv, S) / rel + tab(nA, Ax, Av, S)) * dt; if (S < t) S = t; }
        if (S < 0.0) S = 0.0;
        R -= (tab(nF, Fx, Fv, R) / rel + tab(nA, Ax, Av, R) + (R > S ? grid(nR, Rk, nS, Sk, Ffv, R, S) : 0.0)) * dt;
        if (R < S) R = S;
        if (R < t) R = t;
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
