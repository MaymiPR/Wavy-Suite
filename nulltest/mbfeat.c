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

/* The explicit level detector of the WBassX "det" model: two attack / release followers in cascade,
     e1 = c1 e1 + (1 - c1) |z|,  c1 = a1 if |z| > e1 else r1
     e2 = c2 e2 + (1 - c2) e1,   c2 = a2 if e1 > e2 else r2
   with forward-mode derivatives of e1 wrt (a1, r1) and of e2 wrt (a1, r1, a2, r2), for training the time constants. */
__declspec(dllexport) void mb_follow2(const double* z, int n, double a1, double r1, double a2, double r2,
                                      double* e1o, double* e2o, double* d1, double* d2)
{
    double e1 = 0.0, e2 = 0.0, s1a = 0.0, s1r = 0.0, s2a1 = 0.0, s2r1 = 0.0, s2a2 = 0.0, s2r2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double x = z[i] < 0 ? -z[i] : z[i];
        const int up1 = x > e1;
        const double c1 = up1 ? a1 : r1;
        const double ne1 = c1 * e1 + (1.0 - c1) * x;
        const double ns1a = (up1 ? (e1 - x) : 0.0) + c1 * s1a;
        const double ns1r = (up1 ? 0.0 : (e1 - x)) + c1 * s1r;
        e1 = ne1; s1a = ns1a; s1r = ns1r;
        const int up2 = e1 > e2;
        const double c2 = up2 ? a2 : r2;
        const double ne2 = c2 * e2 + (1.0 - c2) * e1;
        s2a1 = c2 * s2a1 + (1.0 - c2) * s1a;
        s2r1 = c2 * s2r1 + (1.0 - c2) * s1r;
        s2a2 = (up2 ? (e2 - e1) : 0.0) + c2 * s2a2;
        s2r2 = (up2 ? 0.0 : (e2 - e1)) + c2 * s2r2;
        e2 = ne2;
        e1o[i] = e1; e2o[i] = e2;
        d1[2 * i] = s1a; d1[2 * i + 1] = s1r;
        d2[4 * i] = s2a1; d2[4 * i + 1] = s2r1; d2[4 * i + 2] = s2a2; d2[4 * i + 3] = s2r2;
    }
}
