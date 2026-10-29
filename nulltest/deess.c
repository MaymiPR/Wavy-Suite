/* WVox Chain's de-esser model for fitting (loaded with ctypes by fit_voxchain_deess.py; built by build_deess.bat).
   A split-band de-esser as measured on the reference (MEASUREMENTS.md, WVox Chain): the high band (a second-order
   high-pass) is turned down, the rest (the input less the high band) passes; stereo-linked; the threshold rises with
   the low band's level (adaptive). Parameters p[]:
     0 fc (Hz)  1 Q  2 threshold (dB)  3 slope (dB of reduction per dB over)  4 knee (dB)  5 range (dB, > 0)
     6 high attack (ms)  7 high release (ms)  8 adaptive offset (dB)  9 low attack (ms)  10 low release (ms)
     11 adaptive weight (0 .. 1)  12 detector: 0 peak, 1 rms-ish (the power followed)  13 hold (ms)
     14 detector high-pass fc (Hz, 0: the band itself)  15 detector Q */
#include <math.h>

typedef struct { double b0, b1, b2, a1, a2, z1, z2; } bq;
static double bq_run(bq* f, double x) { double y = f->b0 * x + f->z1; f->z1 = f->b1 * x - f->a1 * y + f->z2; f->z2 = f->b2 * x - f->a2 * y; return y; }
static void hp_set(bq* f, double fc, double q, double fs)
{
    double w = 2 * 3.14159265358979323846 * fc / fs, cw = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
    f->b0 = (1 + cw) / 2 / a0; f->b1 = -(1 + cw) / a0; f->b2 = (1 + cw) / 2 / a0; f->a1 = -2 * cw / a0; f->a2 = (1 - al) / a0;
    f->z1 = f->z2 = 0;
}
static double coef(double ms, double fs) { return ms <= 0 ? 0 : exp(-1.0 / (ms * 0.001 * fs)); }

__declspec(dllexport) void deess(const double* xl, const double* xr, double* yl, double* yr, double* gr, int n, double fs, const double* p)
{
    bq hl, hr, dl, dr;
    hp_set(&hl, p[0], p[1], fs); hp_set(&hr, p[0], p[1], fs);
    int sep = p[14] > 0;
    if (sep) { hp_set(&dl, p[14], p[15], fs); hp_set(&dr, p[14], p[15], fs); }
    double ha = coef(p[6], fs), hrl = coef(p[7], fs), la = coef(p[9], fs), lr = coef(p[10], fs);
    double eh = 0, el = 0, hold = 0;
    int holdN = (int) (p[13] * 0.001 * fs);
    for (int i = 0; i < n; ++i)
    {
        double hL = bq_run(&hl, xl[i]), hR = bq_run(&hr, xr[i]);
        double dL = sep ? bq_run(&dl, xl[i]) : hL, dR = sep ? bq_run(&dr, xr[i]) : hR;
        double lL = xl[i] - hL, lR = xr[i] - hR;
        double vh = p[12] > 0.5 ? (dL * dL > dR * dR ? dL * dL : dR * dR) : (fabs(dL) > fabs(dR) ? fabs(dL) : fabs(dR));
        double vl = p[12] > 0.5 ? (lL * lL > lR * lR ? lL * lL : lR * lR) : (fabs(lL) > fabs(lR) ? fabs(lL) : fabs(lR));
        if (vh > eh) { eh = ha * eh + (1 - ha) * vh; hold = holdN; }
        else if (hold > 0) hold -= 1;
        else eh = hrl * eh + (1 - hrl) * vh;
        el = (vl > el ? la : lr) * el + (1 - (vl > el ? la : lr)) * vl;
        double LH = (p[12] > 0.5 ? 10 : 20) * log10(eh + 1e-30), LL = (p[12] > 0.5 ? 10 : 20) * log10(el + 1e-30);
        double thr = p[2];
        double ad = LL + p[8];
        if (ad > thr) thr = thr + p[11] * (ad - thr);                 /* the adaptive threshold */
        double over = LH - thr, k = p[4], r;
        if (k > 0 && over > -k / 2 && over < k / 2) r = p[3] * (over + k / 2) * (over + k / 2) / (2 * k);
        else r = over > 0 ? p[3] * over : 0;
        if (r > p[5]) r = p[5];
        double g = pow(10.0, -r / 20);
        yl[i] = lL + g * hL; yr[i] = lR + g * hR;
        if (gr) gr[i] = r;
    }
}
