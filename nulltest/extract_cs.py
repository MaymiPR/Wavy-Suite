"""Recovers the time-varying gain of the RBass generated path by least squares.

With Decay 0 the generated part (split off with the Intensity render) is modelled as   g = H_post[ c(t) * s(t) ]
where s is a known band signal of the input and H_post the known filters after the gain. c(t) is a piecewise-linear
function with knots every K samples; the problem is linear in the knot values, solved with LSQR plus a light
second-difference penalty. The relative residual tells whether the assumed placement (s, H_post) is right.

    from extract_cs import extract;  c, t, resid_db = extract(x, g, F, placement)
"""
import numpy as np
from scipy.signal import sosfilt
from scipy.sparse import coo_matrix, vstack, diags
from scipy.sparse.linalg import lsqr
import wbass_model as M

FS = 48000


def chains(F, placement, x):
    l = sosfilt(M.sos(3, 'low', F / 3), x)
    hp = M.sos(3, 'high', F)
    if placement == "pre":       # c * l, then HP3 HP3
        return l, [hp, hp]
    if placement == "post":      # c * HP3(l), then HP3
        return sosfilt(hp, l), [hp]
    if placement == "out":       # HP3 HP3 l, then c
        return sosfilt(hp, sosfilt(hp, l)), []
    if placement == "mid1":      # HP1 section, c, HP2 section, HP3
        return sosfilt(M.sec('high', 1, F), l), [M.sec('high', 2, F), hp]
    if placement == "mid2":      # HP2 section, c, HP1 section, HP3
        return sosfilt(M.sec('high', 2, F), l), [M.sec('high', 1, F), hp]
    raise ValueError(placement)


def extract(x, g, F, placement="pre", K=24, start=0, stop=None, lam=1e-4, warm=4800):
    """Dense least squares over [start, stop); the filters run from `start - warm` with c fixed at the first knot value
    estimate so earlier history does not leak in: the window should start in a steady part."""
    x = np.asarray(x, np.float64); g = np.asarray(g, np.float64)
    s, post = chains(F, placement, x)
    stop = len(x) if stop is None else stop
    n = stop - start; nk = n // K + 1
    A = np.zeros((n, nk))
    for j in range(nk):
        t = np.arange(n); hat = np.clip(1.0 - np.abs(t - j * K) / K, 0, None)
        seg = hat * s[start:stop]
        for p in post: seg = sosfilt(p, seg)
        A[:, j] = seg
    # history before the window: the part of g caused by c*s before `start` is not modelled; drop the first warm samples
    y = g[start:stop]
    w0 = min(warm, n // 3)
    Aw, yw = A[w0:], y[w0:]
    scale = np.sqrt(np.mean(yw ** 2))
    D = np.diff(np.eye(nk), 2, axis=0)
    cn = np.sqrt((Aw ** 2).sum(0).mean())
    M_ = np.vstack([Aw, lam * cn * D]); Y = np.concatenate([yw, np.zeros(nk - 2)])
    c, *_ = np.linalg.lstsq(M_, Y, rcond=None)
    r = Aw @ c - yw
    return c, start + np.arange(nk) * K, 20 * np.log10(np.sqrt(np.mean(r ** 2)) / scale)


def extract2(x, g, F, K1=48, K2=48, start=0, stop=None, lam=1e-2, warm=4800, iters=25, inner="pre"):
    """Two gains: g = c2(t) * POST[ c1(t) * s ], alternating least squares. Returns c1, c2 (knot values), knot times,
    residual history (dB)."""
    x = np.asarray(x, np.float64); g = np.asarray(g, np.float64)
    s, post = chains(F, inner, x)
    stop = len(x) if stop is None else stop
    n = stop - start
    def hats(K):
        nk = n // K + 1; t = np.arange(n)
        return np.stack([np.clip(1.0 - np.abs(t - j * K) / K, 0, None) for j in range(nk)], 1)
    H1, H2 = hats(K1), hats(K2)
    A1 = np.zeros_like(H1)
    for j in range(H1.shape[1]):
        seg = H1[:, j] * s[start:stop]
        for p in post: seg = sosfilt(p, seg)
        A1[:, j] = seg
    y = g[start:stop]; w0 = min(warm, n // 3); yw = y[w0:]; scale = np.sqrt(np.mean(yw ** 2))
    def solve(A, nk, target):
        D = np.diff(np.eye(nk), 2, axis=0); cn = np.sqrt((A ** 2).sum(0).mean())
        c, *_ = np.linalg.lstsq(np.vstack([A, lam * cn * D]), np.concatenate([target, np.zeros(nk - 2)]), rcond=None)
        return c
    c2 = np.ones(H2.shape[1]); hist = []
    for it in range(iters):
        g2 = H2 @ c2
        c1 = solve((A1 * g2[:, None])[w0:], H1.shape[1], yw)
        inner_sig = A1 @ c1
        c2 = solve((H2 * inner_sig[:, None])[w0:], H2.shape[1], yw)
        r = (H2 @ c2 * inner_sig)[w0:] - yw
        hist.append(20 * np.log10(np.sqrt(np.mean(r ** 2)) / scale))
    m = np.mean(H2 @ c2); c1 *= m; c2 /= m          # put the overall scale into c1
    return c1, c2, start + np.arange(H1.shape[1]) * K1, start + np.arange(H2.shape[1]) * K2, hist
