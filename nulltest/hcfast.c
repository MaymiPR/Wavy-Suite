/* WComp Phase 4: H-Comp's gain path as identified so far, per sample (loaded with ctypes by hcmodel.py).

   a:  the detector input |x| (linked: the larger channel), n samples; g: the gain out (linear, without makeup).
   p:  parameters, see hcmodel.py (P_*). The structure:
       h  = max a[n + lead - D .. n + lead - D + H - 1]                  look-ahead peak hold (D: Punch)
       Ef = fast envelope: rises to h at once, falls with its release law
       Es = slow envelope: charges toward h, falls with the Release control's law
       E  = max(Ef, Es)
       gt = 10^(static(E) / 20)                                          soft-knee curve, floor eps
       g1 = one-pole on the linear gain: attack coefficient falling, release coefficient rising
       g  = box(B1) * box(B2) of g1                                     the smoothing filter
   Release laws ("mode" bits): the reciprocal 1/E rises by a constant amount per sample (rate), or E decays as a
   one-pole on the linear level. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { P_THR, P_RATIO, P_EPS, P_KNEE, P_TOFF, P_ATT_C, P_REL_C, P_FAST_RATE, P_SLOW_RATE, P_CHARGE_C, P_MODE, P_D, P_H, P_LEAD,
       P_B1, P_B2, P_FAST_EXP, P_COUNT };

static double static_db(double E, const double* p)
{
    const double o = 20.0 * log10(E + p[P_EPS]) - (p[P_THR] + p[P_TOFF]);
    const double w = p[P_KNEE];
    double k;
    if (o <= -w * 0.5) k = 0.0;
    else if (o >= w * 0.5) k = o;
    else k = (o + w * 0.5) * (o + w * 0.5) / (2.0 * w);
    return -(1.0 - 1.0 / p[P_RATIO]) * k;
}

__declspec(dllexport) void hc_gain(const double* a, double* g, double* trace, int n, const double* p)
{
    const int D = (int) p[P_D], H = (int) p[P_H], lead = (int) p[P_LEAD], B1 = (int) p[P_B1], B2 = (int) p[P_B2];
    const int mode = (int) p[P_MODE];
    double* h = (double*) malloc(sizeof(double) * n);
    /* look-ahead hold (simple O(nH): fine for identification) */
    for (int i = 0; i < n; ++i)
    {
        double m = 0.0;
        for (int k = 0; k < H; ++k)
        {
            int j = i + lead - D + k;
            if (j < 0) j = 0;
            if (j >= n) j = n - 1;
            if (a[j] > m) m = a[j];
        }
        h[i] = m;
    }
    double Ef = h[0], Es = h[0], g1 = 1.0;
    double* s1 = (double*) calloc(B1, sizeof(double));
    double* s2 = (double*) calloc(B2, sizeof(double));
    double sum1 = 0.0, sum2 = 0.0;
    int i1 = 0, i2 = 0;
    {
        const double g0 = pow(10.0, static_db(h[0], p) / 20.0);
        g1 = g0;
        for (int k = 0; k < B1; ++k) s1[k] = g0;
        sum1 = g0 * B1;
        for (int k = 0; k < B2; ++k) s2[k] = g0;
        sum2 = g0 * B2;
    }
    for (int i = 0; i < n; ++i)
    {
        const double x = h[i];
        /* fast envelope */
        if (x >= Ef) Ef = x;
        else if (mode & 1) Ef = 1.0 / (1.0 / Ef + p[P_FAST_RATE] * pow(Ef, p[P_FAST_EXP]));
        else Ef = x + (Ef - x) * p[P_FAST_RATE];
        if (Ef < x) Ef = x;
        /* slow envelope */
        if (x >= Es) Es = x + (Es - x) * p[P_CHARGE_C];
        else if (mode & 2) Es = 1.0 / (1.0 / Es + p[P_SLOW_RATE]);
        else Es = x + (Es - x) * p[P_SLOW_RATE];
        if (Es < x && (mode & 4)) Es = x;
        const double E = Ef > Es ? Ef : Es;
        const double gt = pow(10.0, static_db(E, p) / 20.0);
        g1 = gt + ((gt < g1) ? p[P_ATT_C] : p[P_REL_C]) * (g1 - gt);
        sum1 += g1 - s1[i1]; s1[i1] = g1; i1 = (i1 + 1) % B1;
        const double v1 = sum1 / B1;
        sum2 += v1 - s2[i2]; s2[i2] = v1; i2 = (i2 + 1) % B2;
        g[i] = sum2 / B2;
        if (trace) { trace[3 * i] = Ef; trace[3 * i + 1] = Es; trace[3 * i + 2] = gt; }
    }
    free(h); free(s1); free(s2);
}
