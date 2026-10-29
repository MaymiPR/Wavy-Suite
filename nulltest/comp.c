/* WVox Chain's Compress model for fitting (loaded with ctypes by fit_voxchain_compress.py; built by build_comp.bat).
   As measured on the reference (MEASUREMENTS.md, WVox Chain): an input gain into a compressor, then a look-ahead
   limiter whose ceiling holds the output flat, then an output gain; stereo-linked. Parameters p[]:
     0 input gain (dB)  1 threshold (dB)  2 ratio  3 knee (dB)  4 attack (ms)  5 release (ms)  6 slow release (ms)
     7 slow release's share (0 .. 1)  8 ceiling (dB, at the limiter)  9 limiter release (ms)  10 look-ahead (samples)
     11 output gain (dB)  12 detector: 0 peak, 1 power  13 power window (ms)  14 limiter knee (dB)
     15 a2 (x^2)  16 b (x |x|)  17 a3 (x^3): a static shaper  18 its place: 0 before the compressor, 1 after it
     19 .. 36 a static correction of the reduction (dB) at the input-referred detector levels -45, -42 .. +6 dB (linear
     between them, held beyond), solved from the tone ladder (fit_voxchain_compress.py)
     37 the order: 0 the compressor then the limiter; 1 the limiter first (as the chain's L1 before its 1176: then the
        input gain p[0] sits between them, and the limiter has its own input gain p[39])
     38 the compressor's detector: 0 on its input (feed-forward), 1 on its output (feedback)
     39 the limiter's input gain (dB; the limiter-first order only)
   The output is aligned with the input (the look-ahead's delay taken out). */
#include <math.h>
#include <stdlib.h>

static double coef(double ms, double fs) { return ms <= 0 ? 0 : exp(-1.0 / (ms * 0.001 * fs)); }

#define SHAPE(u) ((u) + a2 * (u) * (u) + bb * (u) * fabs(u) + a3 * (u) * (u) * (u))

/* the compressor, in place; gin applied first; the correction's knots referred to the level before gin */
static void compressor(double* a, double* b, int n, double fs, const double* p, double gindb, int shapeBefore, int shapeAfter)
{
    const double gin = pow(10.0, gindb / 20);
    const double T = p[1], R = p[2] < 1 ? 1 : p[2], K = p[3];
    const double ca = coef(p[4], fs), cr = coef(p[5], fs), cs = coef(p[6], fs), share = p[7], cw = coef(p[13], fs);
    const double a2 = p[15], bb = p[16], a3 = p[17];
    const int fb = p[38] > 0.5;
    double gf = 0, gsl = 0, pw = 0, ol = 0, orr = 0;
    for (int i = 0; i < n; ++i)
    {
        double x = a[i] * gin, y = b[i] * gin;
        if (shapeBefore) { x = SHAPE(x); y = SHAPE(y); }
        double v = fb ? (fabs(ol) > fabs(orr) ? fabs(ol) : fabs(orr)) : (fabs(x) > fabs(y) ? fabs(x) : fabs(y));
        double L;
        if (p[12] > 0.5) { pw = cw * pw + (1 - cw) * v * v; L = 10 * log10(pw + 1e-30); }
        else L = 20 * log10(v + 1e-30);
        double over = L - T, t;                                    /* the static reduction (dB) */
        if (K > 0 && over > -K / 2 && over < K / 2) t = (1 - 1 / R) * (over + K / 2) * (over + K / 2) / (2 * K);
        else t = over > 0 ? (1 - 1 / R) * over : 0;
        {
            double u = (L - gindb + 45.0) / 3.0;                   /* the correction's knots (input-referred) */
            if (u <= 0) t += p[19];
            else if (u >= 17) t += p[36];
            else { int k = (int) u; double f = u - k; t += p[19 + k] + (p[20 + k] - p[19 + k]) * f; }
        }
        if (t > gf) gf = ca * gf + (1 - ca) * t; else gf = cr * gf + (1 - cr) * t;
        if (t > gsl) gsl = ca * gsl + (1 - ca) * t; else gsl = cs * gsl + (1 - cs) * t;
        double g = pow(10.0, -((1 - share) * gf + share * gsl) / 20);
        x *= g; y *= g;
        ol = x; orr = y;
        if (shapeAfter) { x = SHAPE(x); y = SHAPE(y); }
        a[i] = x; b[i] = y;
    }
}

/* the look-ahead limiter, in place and aligned: the gain each sample needs to stay under the ceiling (a soft knee
   below it), the lowest over the look-ahead taken at once, released slowly */
static void limiter(double* a, double* b, int n, double fs, const double* p, double gindb)
{
    const double gin = pow(10.0, gindb / 20), ceil = pow(10.0, p[8] / 20), clr = coef(p[9], fs), lk = p[14];
    int la = (int) p[10]; if (la < 0) la = 0; if (la > 4096) la = 4096;
    double* bl = (double*) calloc((size_t) la + 1, sizeof(double));
    double* br = (double*) calloc((size_t) la + 1, sizeof(double));
    int* dq = (int*) calloc((size_t) n + la + 2, sizeof(int));
    double* nd = (double*) calloc((size_t) n + la + 2, sizeof(double));
    int qh = 0, qt = 0, w = 0;
    double lg = 1;
    for (int i = 0; i < n + la; ++i)
    {
        double x = i < n ? a[i] * gin : 0, y = i < n ? b[i] * gin : 0;
        double pk = fabs(x) > fabs(y) ? fabs(x) : fabs(y), need = 1;
        if (pk > 1e-30)
        {
            double over2 = 20 * log10(pk / ceil);
            double r2 = lk > 0 && over2 > -lk / 2 && over2 < lk / 2 ? (over2 + lk / 2) * (over2 + lk / 2) / (2 * lk) : (over2 > 0 ? over2 : 0);
            need = pow(10.0, -r2 / 20);
        }
        bl[w] = x; br[w] = y;
        nd[i] = need;
        while (qt > qh && nd[dq[qt - 1]] >= need) --qt;
        dq[qt++] = i;
        while (dq[qh] < i - la) ++qh;
        double mn = nd[dq[qh]];
        if (mn < lg) lg = mn; else lg = clr * lg + (1 - clr) * mn;
        int rd = (w + 1) % (la + 1);                                /* the oldest sample: la behind */
        int o = i - la;
        if (o >= 0 && o < n) { a[o] = bl[rd] * lg; b[o] = br[rd] * lg; }
        w = (w + 1) % (la + 1);
    }
    free(bl); free(br); free(dq); free(nd);
}

__declspec(dllexport) void comp(const double* xl, const double* xr, double* yl, double* yr, double* gr, int n, double fs, const double* p)
{
    const double gout = pow(10.0, p[11] / 20);
    const int after = p[18] >= 0.5;
    for (int i = 0; i < n; ++i) { yl[i] = xl[i]; yr[i] = xr[i]; }
    if (p[37] < 0.5)
    {
        compressor(yl, yr, n, fs, p, p[0], ! after, after);
        limiter(yl, yr, n, fs, p, 0.0);
    }
    else
    {
        limiter(yl, yr, n, fs, p, p[39]);
        compressor(yl, yr, n, fs, p, p[0], ! after, after);
    }
    for (int i = 0; i < n; ++i) { yl[i] *= gout; yr[i] *= gout; }
    (void) gr;
}
