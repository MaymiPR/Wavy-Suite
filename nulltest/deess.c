/* WVox Chain's main de-esser model for fitting (loaded with ctypes by fit_voxchain_deess.py; built by build_deess.bat).
   The structure measured on the reference (probe_deess.py, MEASUREMENTS.md): the band turned down is a zero-phase
   (linear-phase) high band H; the output is x - (1 - g) H x, so g = 1 passes x untouched. g comes from two detectors on
   the input, stereo-linked: a high one (a second-order high-pass, an attack / release follower) against a threshold
   that rises with a low one (a second-order low-pass, a slow follower), in linear amplitude: thr = T + k el. The
   reduction (dB) is slope x the high level over thr, through a knee, up to the range, then smoothed: attack, a hold,
   a release in the dB or the linear domain, and a slow stage (a second follower of the target that charges during
   long reductions; the reduction is the larger of the two). This file computes the gain g per sample only (the caller
   applies it to H x, with the look-ahead). Parameters p[]:
     0 T (dB)  1 k (amplitude weight of the low level)  2 fh (Hz)  3 Qh  4 fl (Hz)  5 Ql
     6 high attack (ms)  7 high release (ms)  8 low attack (ms)  9 low release (ms)
     10 slope  11 range (dB)  12 knee (dB)
     13 gain attack (ms)  14 hold (ms)  15 gain release (ms)  16 release domain (0 dB, 1 linear)
     17 slow attack (ms)  18 slow release (ms)  19 slow share (0: off)
     20 detector power (0 |x|, 1 x^2) */
#include <math.h>

typedef struct { double b0, b1, b2, a1, a2, z1[2], z2[2]; } bq;
static double bq_run(bq* f, double x, int c)
{
    double y = f->b0 * x + f->z1[c];
    f->z1[c] = f->b1 * x - f->a1 * y + f->z2[c];
    f->z2[c] = f->b2 * x - f->a2 * y;
    return y;
}
static void rbj(bq* f, int high, double fc, double q, double fs)
{
    double w = 2 * 3.14159265358979323846 * fc / fs, cw = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
    if (high) { f->b0 = (1 + cw) / 2 / a0; f->b1 = -(1 + cw) / a0; f->b2 = (1 + cw) / 2 / a0; }
    else { f->b0 = (1 - cw) / 2 / a0; f->b1 = (1 - cw) / a0; f->b2 = (1 - cw) / 2 / a0; }
    f->a1 = -2 * cw / a0; f->a2 = (1 - al) / a0;
    f->z1[0] = f->z1[1] = f->z2[0] = f->z2[1] = 0;
}
static double coef(double ms, double fs) { return ms <= 0 ? 0 : exp(-1.0 / (ms * 0.001 * fs)); }

__declspec(dllexport) void deess(const double* xl, const double* xr, double* g, int n, double fs, const double* p)
{
    bq dh, dl;
    rbj(&dh, 1, p[2], p[3], fs); rbj(&dl, 0, p[4], p[5], fs);
    const int pw = p[20] > 0.5;
    const double ha = coef(p[6], fs), hr = coef(p[7], fs), la = coef(p[8], fs), lr = coef(p[9], fs);
    const double T = pow(10.0, p[0] / 20), k = p[1], slope = p[10], range = p[11], knee = p[12];
    const double ga = coef(p[13], fs), gr = coef(p[15], fs), sa = coef(p[17], fs), sr = coef(p[18], fs), share = p[19];
    const int holdN = (int) (p[14] * 0.001 * fs), lin = p[16] > 0.5;
    double eh = 0, el = 0, r = 0, rs = 0, gl = 1;
    int hold = 0;
    for (int i = 0; i < n; ++i)
    {
        double hL = bq_run(&dh, xl[i], 0), hR = bq_run(&dh, xr[i], 1);
        double lL = bq_run(&dl, xl[i], 0), lR = bq_run(&dl, xr[i], 1);
        double vh = fabs(hL) > fabs(hR) ? fabs(hL) : fabs(hR), vl = fabs(lL) > fabs(lR) ? fabs(lL) : fabs(lR);
        if (pw) { vh *= vh; vl *= vl; }
        eh = (vh > eh ? ha : hr) * eh + (1 - (vh > eh ? ha : hr)) * vh;
        el = (vl > el ? la : lr) * el + (1 - (vl > el ? la : lr)) * vl;
        double H = pw ? sqrt(eh) : eh, L = pw ? sqrt(el) : el;
        double over = 20 * log10((H + 1e-30) / (T + k * L)), rt;
        if (knee > 0 && over > -knee / 2 && over < knee / 2) rt = slope * (over + knee / 2) * (over + knee / 2) / (2 * knee);
        else rt = over > 0 ? slope * over : 0;
        if (rt > range) rt = range;
        /* the slow stage */
        rs = (rt > rs ? sa : sr) * rs + (1 - (rt > rs ? sa : sr)) * rt;
        double tgt = rt > share * rs ? rt : share * rs;
        if (! lin)
        {
            if (tgt >= r) { r = ga * r + (1 - ga) * tgt; hold = holdN; }
            else if (hold > 0) --hold;
            else r = gr * r + (1 - gr) * tgt;
            g[i] = pow(10.0, -r / 20);
        }
        else
        {
            double gt = pow(10.0, -tgt / 20);
            if (gt <= gl) { gl = ga * gl + (1 - ga) * gt; hold = holdN; }
            else if (hold > 0) --hold;
            else gl = gr * gl + (1 - gr) * gt;
            g[i] = gl;
        }
    }
}
