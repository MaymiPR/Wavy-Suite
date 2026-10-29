/* WVox Chain's Compress model for fitting (loaded with ctypes by fit_voxchain_compress.py; built by build_comp.bat).
   As measured on the reference (MEASUREMENTS.md, WVox Chain): an input gain into a compressor, then a look-ahead
   limiter whose ceiling holds the output flat, then an output gain; stereo-linked. Parameters p[]:
     0 input gain (dB)  1 threshold (dB)  2 ratio  3 knee (dB)  4 attack (ms)  5 release (ms)  6 slow release (ms)
     7 slow release's share (0 .. 1)  8 ceiling (dB, at the limiter)  9 limiter release (ms)  10 look-ahead (samples)
     11 output gain (dB)  12 detector: 0 peak, 1 power  13 power window (ms)  14 limiter knee (dB)
     15 a2 (x^2)  16 b (x |x|)  17 a3 (x^3): a static shaper  18 its place: 0 before the compressor, 1 after it
   The output is aligned with the input (the look-ahead's delay taken out); the last p[10] samples are zero. */
#include <math.h>
#include <stdlib.h>

static double coef(double ms, double fs) { return ms <= 0 ? 0 : exp(-1.0 / (ms * 0.001 * fs)); }

__declspec(dllexport) void comp(const double* xl, const double* xr, double* yl, double* yr, double* gr, int n, double fs, const double* p)
{
    const double gin = pow(10.0, p[0] / 20), gout = pow(10.0, p[11] / 20);
    const double T = p[1], R = p[2] < 1 ? 1 : p[2], K = p[3];
    const double ca = coef(p[4], fs), cr = coef(p[5], fs), cs = coef(p[6], fs), share = p[7];
    const double ceil = pow(10.0, p[8] / 20), clr = coef(p[9], fs), cw = coef(p[13], fs), lk = p[14];
    int la = (int) p[10]; if (la < 0) la = 0; if (la > 4096) la = 4096;
    double gf = 0, gsl = 0, pw = 0, lg = 1;                        /* fast / slow reduction (dB), power, limiter gain */
    double* bl = (double*) calloc((size_t) la + 1, sizeof(double));
    double* br = (double*) calloc((size_t) la + 1, sizeof(double));
    double* bm = (double*) calloc((size_t) la + 1, sizeof(double)); /* the needed limiter gain per look-ahead sample */
    int* dq = (int*) calloc((size_t) n + la + 2, sizeof(int));      /* a monotonic queue of indices: the window's minimum */
    double* nd = (double*) calloc((size_t) n + la + 2, sizeof(double));
    int qh = 0, qt = 0;
    int w = 0;
    const double a2 = p[15], bb = p[16], a3 = p[17];
    #define SHAPE(u) ((u) + a2 * (u) * (u) + bb * (u) * fabs(u) + a3 * (u) * (u) * (u))
    for (int i = 0; i < n + la; ++i)
    {
        double a = i < n ? xl[i] * gin : 0, b = i < n ? xr[i] * gin : 0;
        if (p[18] < 0.5) { a = SHAPE(a); b = SHAPE(b); }
        double v = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
        double L;
        if (p[12] > 0.5) { pw = cw * pw + (1 - cw) * v * v; L = 10 * log10(pw + 1e-30); }
        else L = 20 * log10(v + 1e-30);
        double over = L - T, t;                                    /* the static reduction (dB) */
        if (K > 0 && over > -K / 2 && over < K / 2) t = (1 - 1 / R) * (over + K / 2) * (over + K / 2) / (2 * K);
        else t = over > 0 ? (1 - 1 / R) * over : 0;
        if (t > gf) gf = ca * gf + (1 - ca) * t; else gf = cr * gf + (1 - cr) * t;
        if (t > gsl) gsl = ca * gsl + (1 - ca) * t; else gsl = cs * gsl + (1 - cs) * t;
        double red = (1 - share) * gf + share * gsl;
        double g = pow(10.0, -red / 20);
        a *= g; b *= g;
        if (p[18] >= 0.5) { a = SHAPE(a); b = SHAPE(b); }
        /* the limiter: the gain each sample needs to stay under the ceiling (a soft knee below it), the lowest over the
           look-ahead taken at once, released slowly */
        double pk = fabs(a) > fabs(b) ? fabs(a) : fabs(b), need = 1;
        if (pk > 1e-30)
        {
            double over2 = 20 * log10(pk / ceil);
            double r2 = lk > 0 && over2 > -lk / 2 && over2 < lk / 2 ? (over2 + lk / 2) * (over2 + lk / 2) / (2 * lk) : (over2 > 0 ? over2 : 0);
            need = pow(10.0, -r2 / 20);
        }
        bl[w] = a; br[w] = b; bm[w] = need;
        nd[i] = need;
        while (qt > qh && nd[dq[qt - 1]] >= need) --qt;
        dq[qt++] = i;
        while (dq[qh] < i - la) ++qh;
        double mn = nd[dq[qh]];
        if (mn < lg) lg = mn; else lg = clr * lg + (1 - clr) * mn;
        int rd = (w + 1) % (la + 1);                                /* the oldest sample: la behind */
        int o = i - la;
        if (o >= 0 && o < n)
        {
            yl[o] = bl[rd] * lg * gout; yr[o] = br[rd] * lg * gout;
            if (gr) gr[o] = red - 20 * log10(lg);
        }
        w = (w + 1) % (la + 1);
    }
    free(bl); free(br); free(bm); free(dq); free(nd);
}
