/* Per-sample helpers for the WBass identification scripts (loaded with ctypes by wbass_model.py). */
#include <math.h>
__declspec(dllexport) void ar_follow(const double* x, double* y, int n, double aa, double ar, double y0)
{
    double e = y0;
    for (int i = 0; i < n; ++i)
    {
        const double v = x[i];
        const double a = v > e ? aa : ar;
        e = a * e + (1.0 - a) * v;
        y[i] = e;
    }
}
