/* Feature helpers for the WBassX level-control training (loaded with ctypes by train_wbassx.py; the same recursions
   run in WBassXDsp.h): a peak follower with instant attack and one-pole release, and a one-pole power smoother. */
__declspec(dllexport) void mb_peak(const double* x, double* y, int n, double rel)
{
    double e = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double v = x[i] < 0 ? -x[i] : x[i];
        e = v > e ? v : rel * e + (1.0 - rel) * v;
        y[i] = e;
    }
}
__declspec(dllexport) void mb_power(const double* x, double* y, int n, double a)
{
    double e = 0.0;
    for (int i = 0; i < n; ++i) { e = a * e + (1.0 - a) * x[i] * x[i]; y[i] = e; }
}
