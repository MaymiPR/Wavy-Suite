"""Phase 4, the doubler's third pass: each voice measured on click trains through the reference (Pitch at +10, every
other colour clear, the output less the output with Pitch clear; the clicks' send response, the fixed filter, divided
out). Writes its data under reference/wvoxchain/meas/doubler2 (git-ignored).

    python -I fit_voxchain_doubler2.py clicks COLOUR SECONDS [SPACING]   render a click train (default every 1200 samples)
    python -I fit_voxchain_doubler2.py track COLOUR SIDE [ROUNDS]         per click the delay, and the voice's shared
                                                                           response (linear interpolation; alternating
                                                                           least squares)
    python -I fit_voxchain_doubler2.py tri COLOUR SIDE                    the delay track as a triangle-speed curve with
                                                                           the one-sample steps after each turn
    python -I fit_voxchain_doubler2.py stagger COLOUR SECONDS             click trains offset 0, 100, .. 1100 samples
    python -I fit_voxchain_doubler2.py grains COLOUR                      the pitch shifter's taps (Spreader): the smooth
                                                                           curve under the grains and each grain's offset

The findings are in MEASUREMENTS.md ("WVox Chain: Phase 4, the doubler's third pass").
"""
import os
import sys
import glob
import numpy as np
from scipy.optimize import least_squares, minimize_scalar
from scipy.signal import sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cv_ref as C
import fit_voxchain_deess as Fd
FS = 48000
OUT = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'doubler2')
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)


def path(name):
    os.makedirs(OUT, exist_ok=True)
    return os.path.join(OUT, name)


def wet(x, colour):
    """the reference's doubler alone (1 s pre-roll: the model's clock t = n / FS + 1 s)"""
    y0 = C.render(x, FS, {C.NOISE: 0}, C.host(**CLEAR, pitch=10))
    y1 = C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, pitch_color=colour), pitch=10))
    return (y1 - y0).astype(float)


def itri(u):
    """the integral of a unit triangle (0 at u = 0, rising to 1 at u = 1/4, period 1) from 0 to u"""
    n = np.floor(u); r = u - n
    return np.where(r < 0.25, 2 * r * r, np.where(r < 0.75, 0.125 + 2 * (r - 0.25) - 2 * (r * r - 0.0625),
                                                   0.125 + 2 * (r * r - 0.5625) - 4 * (r - 0.75)))


def smooth(p, n):
    """p: delay at t = 0 (samples), swing (natural-log units), rate (Hz), phase (cycles), drift (samples per sample)"""
    t = n / FS + 1.0; u = p[2] * t + p[3]
    return p[0] + p[1] * FS / p[2] * (itri(u) - itri(p[3])) + p[4] * FS * t


def with_steps(p, N):
    """the smooth delay plus the reference's extra sample: from the first whole-sample crossing after a turn down
    until the first after the next turn up"""
    d = smooth(p, np.arange(N, dtype=float)); fl = np.floor(d)
    rev = np.where(np.diff(np.sign(np.diff(d))) != 0)[0] + 1
    starts = []
    for r in rev:
        j = r + 1
        while j < N and fl[j] == fl[r]: j += 1
        starts.append(j)
    extra = np.zeros(N)
    for a, (r, j) in enumerate(zip(rev, starts)):
        end = starts[a + 1] if a + 1 < len(starts) else N
        if d[min(r + 1, N - 1)] < d[r]: extra[j:end] = 1
    return d + extra


def deconvolved(w, side, P, length):
    """per click, the response with the clicks' send (the fixed filter) divided out"""
    N = 8192
    imp = np.zeros(P); imp[0] = 0.5
    k0 = np.zeros(N); k0[:P] = sosfilt(Fd.fixed_sos(), imp)
    K = np.fft.rfft(k0); reg = 1e-4 * np.max(np.abs(K)) ** 2
    H = []
    for k in range(1, w.shape[1] // P - 1):
        seg = np.zeros(N); seg[:P] = w[side, k * P:k * P + P]
        H.append(np.fft.irfft(np.fft.rfft(seg) * np.conj(K) / (np.abs(K) ** 2 + reg), N)[:length])
    return np.array(H)


def clicks(colour, secs, P=1200):
    x = np.zeros(int(FS * secs), np.float32); x[P::P] = 0.5
    np.savez(path('clicks%d_%g.npz' % (colour, secs)), w=wet(x, colour), P=P)


def track(colour, side, rounds=25, secs=8):
    z = np.load(path('clicks%d_%g.npz' % (colour, secs))); w, P = z['w'], int(z['P'])
    L, T = int(P * 0.95), 96
    H = deconvolved(w, side, P, L)

    def basis(D):
        i = int(np.floor(D)); f = D - i; M = np.zeros((L, T))
        for j in range(T):
            if i + j < L: M[i + j, j] += 1 - f
            if i + j + 1 < L: M[i + j + 1, j] += f
        return M
    Ds = np.array([float(np.argmax(np.abs(h))) for h in H]); hs = np.zeros(T); hs[0] = 1.8
    for it in range(rounds):
        hs = np.linalg.lstsq(np.concatenate([basis(D) for D in Ds]), H.reshape(-1), rcond=None)[0]
        errs = []
        for k, h in enumerate(H):
            r = minimize_scalar(lambda D: np.sum((h - basis(D) @ hs) ** 2), bounds=(Ds[k] - 1.5, Ds[k] + 1.5), method='bounded',
                                options=dict(xatol=1e-5))
            Ds[k] = r.x; errs.append(r.fun / np.sum(h ** 2))
        print('round %d: median residual %.1f dB, worst %.1f' % (it, 10 * np.log10(np.median(errs)), 10 * np.log10(max(errs))), flush=True)
    np.savez(path('track%d%s.npz' % (colour, 'LR'[side])), Ds=Ds, hs=hs, P=P)


def tri(colour, side):
    z = np.load(path('track%d%s.npz' % (colour, 'LR'[side]))); Ds, P = z['Ds'], int(z['P'])
    idx = np.arange(1, len(Ds) + 1) * P; N = (len(Ds) + 2) * P + 4000

    def res(q):                                  # (the delay as heard at the output sample: the click plus its delay)
        return np.interp(idx + Ds, np.arange(N), with_steps(q, N)) - Ds
    d2 = np.median(np.abs(np.diff(Ds, 2)))
    best = None
    for f0 in (0.15, 0.2, 0.25, 0.3):
        for sgn in (1, -1):
            s = least_squares(res, [Ds[0], sgn * d2 * FS / (4 * f0 * P * P), f0, 0.0, -8e-6], x_scale=[1, 1e-5, 1e-4, 1e-4, 1e-7],
                              diff_step=1e-8, max_nfev=300)
            if best is None or s.cost < best.cost: best = s
    r = res(best.x); q = best.x
    print('rms %.4f max %.4f samples: delay %.4f, swing %.7f (%.3f cents), rate %.6f Hz, phase %.5f cycles (%.1f samples), '
          'drift %.4e' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), q[0], q[1], q[1] / np.log(2) * 1200, q[2], q[3],
                          q[3] / q[2] * FS, q[4]))


def stagger(colour, secs, P=1200, step=100):
    for off in range(0, P, step):
        x = np.zeros(int(FS * secs), np.float32); x[P + off::P] = 0.5
        np.save(path('stagger%d_%d.npy' % (colour, off)), wet(x, colour))


def taps_of(colour, P=1200, threshold=0.004):
    """per click, its taps: linear-interpolation pairs grouped, (delay, gain); Spreader has no shelves"""
    rows = {0: [], 1: []}
    for f in glob.glob(path('stagger%d_*.npy' % colour)):
        off = int(f.split('_')[-1][:-4]); w = np.load(f)
        for side in (0, 1):
            H = deconvolved(w[:, off:], side, P, P - 50)
            for k, h in enumerate(H):
                h = h / 1.2536
                nz = np.where(np.abs(h) > threshold)[0]; taps = []; i = 0
                while i < len(nz):
                    j = i
                    while j + 1 < len(nz) and nz[j + 1] == nz[j] + 1: j += 1
                    ix = nz[i:j + 1]; g = h[ix].sum()
                    taps.append((np.dot(ix, h[ix]) / g if abs(g) > 1e-9 else ix[0], g)); i = j + 1
                rows[side].append(((k + 1) * P + off, taps))
    for side in rows: rows[side].sort(key=lambda r: r[0])
    return rows


def grains(colour):
    rows = taps_of(colour)
    for side in (0, 1):
        tracks, active = [], []
        for c, tp in rows[side]:
            used = set()
            for tr in active:
                best = None
                for k, (p, g) in enumerate(tp):
                    if k not in used and abs(p - tr[-1][1]) < 3 and (best is None or abs(p - tr[-1][1]) < abs(tp[best][0] - tr[-1][1])): best = k
                if best is not None: used.add(best); tr.append((c, tp[best][0], tp[best][1]))
            for k, (p, g) in enumerate(tp):
                if k not in used: tr = [(c, p, g)]; tracks.append(tr); active.append(tr)
            active = [tr for tr in active if tr[-1][0] == c]
        trs = [np.array(tr) for tr in tracks if len(tr) > 20]
        Pp = np.concatenate([tr[:, 1] for tr in trs]); n = np.concatenate([tr[:, 0] for tr in trs]) + Pp
        G = np.concatenate([tr[:, 2] for tr in trs]); K = np.concatenate([np.full(len(tr), i) for i, tr in enumerate(trs)])
        sel = G > 0.98

        def S(q, n):
            u = q[2] * (n + q[3]) / FS
            return q[0] * n + q[1] * FS / q[2] * (itri(u) - itri(q[2] * q[3] / FS))

        def res(q): return (S(q, n) + q[4:][K] - Pp)[sel]
        best = None
        for f0 in (0.1, 0.15, 0.2):
            for b0 in (-0.004, 0.004):
                for A0 in (0.008, -0.008):
                    q0 = np.concatenate([[b0, A0, f0, 0.0], [tr[0, 1] for tr in trs]])
                    s = least_squares(res, q0, x_scale=np.concatenate([[1e-4, 1e-4, 1e-3, 100], np.ones(len(trs))]), max_nfev=300)
                    if best is None or s.cost < best.cost: best = s
        q = best.x; r = res(q)
        print('%s: rms %.4f samples; drift %.3e, swing %.6f (%.2f cents), rate %.6f Hz, LFO lead %.1f samples; grain '
              'offsets %s' % ('LR'[side], np.sqrt(np.mean(r ** 2)), q[0], q[1], q[1] / np.log(2) * 1200, q[2], q[3] - FS,
                              ' '.join('%.2f' % v for v in q[4:])))


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'clicks': clicks(int(a[1]), float(a[2]), int(a[3]) if len(a) > 3 else 1200)
    elif a[0] == 'track': track(int(a[1]), 'LR'.index(a[2]), int(a[3]) if len(a) > 3 else 25)
    elif a[0] == 'tri': tri(int(a[1]), 'LR'.index(a[2]))
    elif a[0] == 'stagger': stagger(int(a[1]), float(a[2]))
    elif a[0] == 'grains': grains(int(a[1]))
