/* Per-sample helpers for the WBassX identification scripts (loaded with ctypes by mbmodel.py). */
__declspec(dllexport) void mb_generator(const double* w, double* y, int n, double a, double b0, double b1, double b2, double a1, double a2, double floor_)
{
    double z1 = 0.0, z2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double den = 1.0 - a * b0 * w[i];
        if (den < floor_) den = floor_;
        const double yn = w[i] * (1.0 + a * z1) / den;
        const double v = b0 * yn + z1;
        z1 = b1 * yn - a1 * v + z2;
        z2 = b2 * yn - a2 * v;
        y[i] = yn;
    }
}

/* The generator's exact linearisation around a trajectory (for the gain extraction). mb_traj stores, per sample, the
   local derivatives dy/dw and dy/dz1 of y = w (1 + a z1) / max(1 - a b0 w, floor); then
   mb_jvp: dy = J dw (forward recursion through the HP_B state), mb_vjp: dw = J^T dy (adjoint, backward). */
__declspec(dllexport) void mb_traj(const double* w, double* y, double* dw, double* dz, int n, double a, double b0, double b1, double b2, double a1, double a2, double floor_)
{
    double z1 = 0.0, z2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double den = 1.0 - a * b0 * w[i];
        const int clipped = den < floor_;
        if (clipped) den = floor_;
        const double num = 1.0 + a * z1;
        const double yn = w[i] * num / den;
        dw[i] = clipped ? num / den : num / (den * den);
        dz[i] = a * w[i] / den;
        const double v = b0 * yn + z1;
        z1 = b1 * yn - a1 * v + z2;
        z2 = b2 * yn - a2 * v;
        y[i] = yn;
    }
}

__declspec(dllexport) void mb_jvp(const double* dw, const double* dz, const double* in, double* out, int n, double b0, double b1, double b2, double a1, double a2)
{
    double z1 = 0.0, z2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double yn = dw[i] * in[i] + dz[i] * z1;
        const double v = b0 * yn + z1;
        const double nz1 = b1 * yn - a1 * v + z2;
        z2 = b2 * yn - a2 * v;
        z1 = nz1;
        out[i] = yn;
    }
}

__declspec(dllexport) void mb_vjp(const double* dw, const double* dz, const double* in, double* out, int n, double b0, double b1, double b2, double a1, double a2)
{
    /* forward: y = dw x + dz z1;  z1' = (b1 - a1 b0) y + (-a1) z1 + z2;  z2' = (b2 - a2 b0) y + (-a2) z1 */
    const double c1 = b1 - a1 * b0, c2 = b2 - a2 * b0;
    double m1 = 0.0, m2 = 0.0;   /* adjoints of z1, z2 at n+1 */
    for (int i = n - 1; i >= 0; --i)
    {
        const double Y = in[i] + m1 * c1 + m2 * c2;
        out[i] = Y * dw[i];
        const double nm1 = Y * dz[i] - a1 * m1 - a2 * m2;
        m2 = m1;
        m1 = nm1;
    }
}

/* The delayed-feedback generator: y[n] = w[n] (1 + a v[n-1]), v = HP_B(y). Like the implicit form for bass, but it
   goes unstable at Nyquist when a b0 |w| > 1, as the reference does. Trajectory with the local derivatives, and the
   exact forward / adjoint linearisations (state z1, z2 of HP_B and the previous output vp). */
__declspec(dllexport) void mbp_traj(const double* w, double* y, double* dw, double* dz, int n, double a, double b0, double b1, double b2, double a1, double a2, double lim)
{
    double z1 = 0.0, z2 = 0.0, vp = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double yn = w[i] * (1.0 + a * vp);
        dw[i] = 1.0 + a * vp; dz[i] = a * w[i];
        if (yn > lim) { yn = lim; dw[i] = 0.0; dz[i] = 0.0; }
        if (yn < -lim) { yn = -lim; dw[i] = 0.0; dz[i] = 0.0; }
        const double v = b0 * yn + z1;
        z1 = b1 * yn - a1 * v + z2;
        z2 = b2 * yn - a2 * v;
        vp = v;
        y[i] = yn;
    }
}

__declspec(dllexport) void mbp_jvp(const double* dw, const double* dz, const double* in, double* out, int n, double b0, double b1, double b2, double a1, double a2)
{
    double z1 = 0.0, z2 = 0.0, vp = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double yn = dw[i] * in[i] + dz[i] * vp;
        const double v = b0 * yn + z1;
        const double nz1 = b1 * yn - a1 * v + z2;
        z2 = b2 * yn - a2 * v;
        z1 = nz1; vp = v;
        out[i] = yn;
    }
}

__declspec(dllexport) void mbp_vjp(const double* dw, const double* dz, const double* in, double* out, int n, double b0, double b1, double b2, double a1, double a2)
{
    /* forward: y = dw x + dz vp;  z1' = c1 y - a1 z1 + z2;  z2' = c2 y - a2 z1;  vp' = b0 y + z1 */
    const double c1 = b1 - a1 * b0, c2 = b2 - a2 * b0;
    double m1 = 0.0, m2 = 0.0, mp = 0.0;
    for (int i = n - 1; i >= 0; --i)
    {
        const double Y = in[i] + m1 * c1 + m2 * c2 + mp * b0;
        out[i] = Y * dw[i];
        const double nm1 = -a1 * m1 - a2 * m2 + mp;
        const double nmp = Y * dz[i];
        m2 = m1; m1 = nm1; mp = nmp;
    }
}
