"""WBassX identification, step 4b: the level-control gain g(t) by forward least squares (no filter inversion).

    G = -K(F) HP_G( y(g u) ),  y = generator (Decay feedback)

Linearised around the current estimate: G ~ -K HP_G( J (g u) ) with J the generator's local slope dy/dw (diagonal
approximation, exact for the linear term), solved for g on hat functions (knots every K samples) with LSQR; the
matrix-vector products are FFT convolutions with the impulse response of HP_G. A few outer iterations re-linearise.

    from extract_g2 import extract;  g, err_db = extract(x, G, F, hp, decay_db)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import scipy.signal as ss
from scipy.sparse.linalg import LinearOperator, lsqr
import mbmodel as MM

FS = 48000


def hat_ops(n, K):
    nk = n // K + 2
    t = np.arange(n); j = t // K; fr = (t - j * K) / K
    def up(c): return (1 - fr) * c[j] + fr * c[j + 1]
    def down(v):
        out = np.zeros(nk); np.add.at(out, j, (1 - fr) * v); np.add.at(out, j + 1, fr * v); return out
    return nk, up, down


def extract(x, G, F, hp, decay_db, K=24, lam=3e-2, outer=4, iters=300, start=2400):
    x = np.asarray(x, np.float64); G = np.asarray(G, np.float64); n = len(x)
    u = ss.sosfilt(MM.lp3(F), x)
    Kout = MM.k_out(F)
    L = 32768                                   # HP_G has decayed by then (-150 dB)
    N = 1 << int(np.ceil(np.log2(n + L)))
    imp = np.zeros(L); imp[0] = 1
    hG = ss.sosfilt(MM.g_highpass(F, hp), imp)
    HG = np.fft.rfft(hG, N)
    conv = lambda v: np.fft.irfft(np.fft.rfft(v, N) * HG, N)[:n]
    corr = lambda v: np.fft.irfft(np.fft.rfft(v, N) * np.conj(HG), N)[:n]
    nk, up, down = hat_ops(n, K)
    D2 = lambda c: c[:-2] - 2 * c[1:-1] + c[2:]
    def D2T(r):
        out = np.zeros(nk); out[:-2] += r; out[1:-1] -= 2 * r; out[2:] += r; return out
    mask = np.zeros(n); mask[start:] = 1
    target = -G / Kout
    g = np.ones(n) * 0.3
    a = 8.0 * 10 ** (decay_db / 20)
    sB = np.atleast_2d(MM.sos2(MM.b_highpass_freq(F), MM.Q2, True))
    c = down(g) / np.maximum(down(np.ones(n)), 1e-12)
    for it in range(outer):
        w = g * u
        y = MM.generator(w, F, decay_db)
        # local slope of y wrt w (the implicit generator's direct term), and the residual of the linearisation
        Jd = (1 + a * ss.sosfilt(sB, y)) / (1 - a * w * sB[0, 0])
        Jd = np.clip(Jd, 0.2, 5.0)
        base = y - Jd * w                    # y ~ base + Jd w
        rhs = (target - conv(base)) * mask
        sc = np.sqrt(np.mean((conv(Jd * u) * mask) ** 2)) + 1e-30
        def mv(cv): return np.r_[conv(Jd * u * up(cv)) * mask, lam * sc * D2(cv)]
        def rmv(r): return down(Jd * u * corr(r[:n] * mask)) + lam * sc * D2T(r[n:])
        A = LinearOperator((n + nk - 2, nk), matvec=mv, rmatvec=rmv)
        c = lsqr(A, np.r_[rhs, np.zeros(nk - 2)], x0=c, iter_lim=iters, atol=1e-10, btol=1e-10)[0]
        g = up(c)
    Gm = -Kout * conv(MM.generator(g * u, F, decay_db))
    sk = max(start, int(0.1 * FS))
    err = 20 * np.log10(np.linalg.norm((Gm - G)[sk:]) / np.linalg.norm(G[sk:]))
    return g, err


if __name__ == "__main__":
    import refhost as R, nulltest_wbass as NW
    S = NW.signals()
    # synthetic check first: a known gain through the model
    x = S["bass notes"][: 2 * FS].astype(np.float64); u = ss.sosfilt(MM.lp3(80), x)
    env = ss.sosfilt(ss.butter(1, 5, fs=FS, output='sos'), np.abs(u)); gt = np.minimum(1.0, 0.05 / (env + 0.01))
    Gs = -MM.k_out(80) * ss.sosfilt(MM.g_highpass(80, 1), MM.generator(gt * u, 80, -24.0))
    g, err = extract(x, Gs, 80, 1, -24.0)
    m = np.abs(u) > 0.2 * np.abs(u).max()
    print("synthetic: re-render", round(err, 1), "dB, g err where |u| big", round(20 * np.log10(np.linalg.norm((g - gt)[m]) / np.linalg.norm(gt[m])), 1), "dB", flush=True)
    for ratio in (1.0, 4.0):
        for name in ("bass steps -50/-10/-30", "bass notes", "kick-like", "sine 50 Hz -12 dBFS"):
            x = S[name][: int(2.0 * FS)].astype(np.float64)
            G = R.render_mb(x.astype(np.float32), FS, harmonics_gain=0.0, monitor=1, ratio=ratio, harmonics_decay=-24.0)[0].astype(np.float64)
            g, err = extract(x, G, 80, 1, -24.0)
            print(f"ratio {ratio} {name:24s} re-render err {err:6.1f} dB   g p1..p99 {np.percentile(g, 1):.3f} .. {np.percentile(g, 99):.3f}", flush=True)
            np.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "renders", "g_r%g_%s.npy" % (ratio, name.replace(" ", "_").replace("/", "-"))), np.stack([x, g]))
