"""WBassX identification, step 4c: the level-control gain g(t) by Gauss-Newton with the generator's exact Jacobian.

    G = -K(F) HP_G( y(w) ),  w = g u,  y = the implicit Decay feedback

Each outer step linearises y around the current w with the exact Jacobian (forward and adjoint recursions through the
HP_B state, mbfast.c) and solves for the knot update with LSQR (HP_G as FFT convolutions), with a light smoothness
penalty on the knots and a step-halving line search.

    from extract_g3 import extract;  g, err_db = extract(x, G, F, hp, decay_db)
"""
import sys, os, ctypes as ct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import scipy.signal as ss
from scipy.sparse.linalg import LinearOperator, lsqr
import mbmodel as MM
from extract_g2 import hat_ops

FS = 48000
_here = os.path.dirname(os.path.abspath(__file__))
for _name in ("mbfast.dll",):
    try:
        _J = ct.CDLL(os.path.join(_here, _name)); _J.mb_traj; break
    except (OSError, AttributeError):
        _J = None
P = ct.c_void_p
_J.mb_traj.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 7
_J.mb_jvp.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 5
_J.mb_vjp.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 5
try:
    _J.mbp_traj.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 7
    _J.mbp_jvp.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 5
    _J.mbp_vjp.argtypes = [P, P, P, P, ct.c_int] + [ct.c_double] * 5
except AttributeError:
    pass
KIND = os.environ.get("MB_GEN", "prev")
GMAX = 1.05      # generator form: "prev" (delayed feedback) or "implicit"


class Gen:
    def __init__(self, F, decay_db, kind=None, lim=1e9):
        self.kind = kind or KIND; self.lim = lim
        self.a = 8.0 * 10 ** (decay_db / 20)
        s = MM.sos2(MM.b_highpass_freq(F), MM.Q2, True)
        self.c = (s[0], s[1], s[2], s[4], s[5])

    def traj(self, w):
        w = np.ascontiguousarray(w, np.float64); n = len(w)
        y, dw, dz = np.empty(n), np.empty(n), np.empty(n)
        if self.kind == "prev": _J.mbp_traj(w.ctypes.data, y.ctypes.data, dw.ctypes.data, dz.ctypes.data, n, self.a, *self.c, self.lim)
        else: _J.mb_traj(w.ctypes.data, y.ctypes.data, dw.ctypes.data, dz.ctypes.data, n, self.a, *self.c, 0.2)
        self.dw, self.dz = dw, dz
        return y

    def jvp(self, v):
        v = np.ascontiguousarray(v, np.float64); out = np.empty_like(v)
        (_J.mbp_jvp if self.kind == "prev" else _J.mb_jvp)(self.dw.ctypes.data, self.dz.ctypes.data, v.ctypes.data, out.ctypes.data, len(v), *self.c)
        return out

    def vjp(self, v):
        v = np.ascontiguousarray(v, np.float64); out = np.empty_like(v)
        (_J.mbp_vjp if self.kind == "prev" else _J.mb_vjp)(self.dw.ctypes.data, self.dz.ctypes.data, v.ctypes.data, out.ctypes.data, len(v), *self.c)
        return out


def extract(x, G, F, hp, decay_db, K=24, lam=1e-2, outer=8, iters=200, start=2400, g0=None):
    x = np.asarray(x, np.float64); G = np.asarray(G, np.float64); n = len(x)
    u = ss.sosfilt(MM.lp3(F), x)
    Kout = MM.k_out(F)
    L = 32768
    N = 1 << int(np.ceil(np.log2(n + L)))
    imp = np.zeros(L); imp[0] = 1
    HG = np.fft.rfft(-Kout * ss.sosfilt(MM.g_highpass(F, hp), imp), N)
    conv = lambda v: np.fft.irfft(np.fft.rfft(v, N) * HG, N)[:n]
    corr = lambda v: np.fft.irfft(np.fft.rfft(v, N) * np.conj(HG), N)[:n]
    nk, up, down = hat_ops(n, K)
    D2 = lambda c: c[:-2] - 2 * c[1:-1] + c[2:]
    def D2T(r):
        out = np.zeros(nk); out[:-2] += r; out[1:-1] -= 2 * r; out[2:] += r; return out
    mask = np.zeros(n); mask[start:] = 1
    gen = Gen(F, decay_db, lim=float(os.environ.get("MB_LIM", "1e9")))
    c = np.full(nk, 0.3) if g0 is None else down(g0) / np.maximum(down(np.ones(n)), 1e-12)
    def cost(cv):
        y = gen.traj(up(cv) * u); r = (G - conv(y)) * mask
        return float(np.sum(r * r) + (lam * sc) ** 2 * np.sum(D2(cv) ** 2)), r
    sc = np.sqrt(np.mean((conv(u) * mask) ** 2)) + 1e-30
    J0, r = cost(c)
    for it in range(outer):
        gen.traj(up(c) * u)                       # linearise here
        def mv(dc): return np.r_[conv(gen.jvp(u * up(dc))) * mask, lam * sc * D2(dc)]
        def rmv(rr): return down(u * gen.vjp(corr(rr[:n] * mask))) + lam * sc * D2T(rr[n:])
        A = LinearOperator((n + nk - 2, nk), matvec=mv, rmatvec=rmv)
        dc = lsqr(A, np.r_[r, -lam * sc * D2(c)], iter_lim=iters, atol=1e-10, btol=1e-10)[0]
        step = 1.0
        for _ in range(6):
            cn = np.clip(c + step * dc, 0.0, GMAX)     # the gain stays within its physical range (cap 1)
            J1, r1 = cost(cn)
            if J1 < J0: break
            step *= 0.5
        if J1 >= J0: break
        c, J0, r = cn, J1, r1
    g = up(c)
    Gm = conv(gen.traj(g * u))
    sk = max(start, int(0.1 * FS))
    err = 20 * np.log10(np.linalg.norm((Gm - G)[sk:]) / np.linalg.norm(G[sk:]))
    return g, err


def _check_jacobian():
    rng = np.random.default_rng(0)
    w = rng.standard_normal(4000) * 0.1; gen = Gen(120, -9.0)
    y0 = gen.traj(w); v = rng.standard_normal(4000); eps = 1e-6
    fd = (Gen(120, -9.0).traj(w + eps * v) - y0) / eps
    print("generator", gen.kind)
    jv = gen.jvp(v)
    q = rng.standard_normal(4000)
    print("JVP vs finite difference: rel err %.2e;  <q, Jv> - <J^T q, v>: %.2e" % (np.linalg.norm(jv - fd) / np.linalg.norm(fd), abs(np.dot(q, jv) - np.dot(gen.vjp(q), v)) / abs(np.dot(q, jv))))


if __name__ == "__main__":
    import glob
    _check_jacobian()
    fs = sorted(glob.glob(os.path.join(_here, "..", "measurements", "maxxbass", "train", "train_*.npz")), key=lambda f: -float(np.load(f)["err"]))[:8]
    for f in fs:
        z = np.load(f)
        x = z["x"].astype(np.float64); G = z["G"].astype(np.float64)
        g, err = extract(x, G, float(z["F"]), int(z["hp"]), float(z["decay"]))
        print(os.path.basename(f), str(z["kind"]), int(z["F"]), float(z["decay"]), float(z["ratio"]), "before %.1f, Gauss-Newton %.1f dB" % (float(z["err"]), err), flush=True)
