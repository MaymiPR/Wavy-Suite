"""Fits H-Comp's GR needle ballistics: the compressor's gain g(t) (pilot_gain.py, 1 ms steps) through a candidate meter
law, sampled at the filmed frames' audio times, quantised to the strip's 71 frames (k = round(70 v)), against the
filmed frame index. Private measurement helper.

    python -I fit_gr_needle.py <capture.npz> [...]
"""
import sys
import numpy as np
from scipy.optimize import minimize

runs = []
for fn in [a for a in sys.argv[1:] if not a.startswith('--')]:
    tg, g = np.load(fn.replace('.npz', '_g.npy'))
    g = np.clip(g / np.median(g[(tg > 8) & (tg < 8.9)]), 1e-4, 1.0)
    t, a, c = np.load(fn.replace('.npz', '_angles.npy'))
    k = np.round((a - 31.75) / (117.17 / 70))
    s = (t > 0.5) & (t < 27.4)
    runs.append((tg, g, t[s], k[s]))
DT = 0.001


def sim(g, law, p):
    out = np.empty_like(g); m = g[0]
    if law == 'exp_db':                  # instant attack, exponential return in dB
        a = np.exp(-DT / p[0]); lg = np.log(g); m = lg[0]
        for i, v in enumerate(lg):
            m = v if v < m else v + a * (m - v); out[i] = m
        return np.exp(out)
    if law == 'exp_lin':                 # instant attack, exponential return in linear gain
        a = np.exp(-DT / p[0])
        for i, v in enumerate(g):
            m = v if v < m else v + a * (m - v); out[i] = m
        return out
    if law == 'exp_db_att':              # exponential attack and return in dB
        aa, ar = np.exp(-DT / p[1]), np.exp(-DT / p[0]); lg = np.log(g); m = lg[0]
        for i, v in enumerate(lg):
            m = v + (aa if v < m else ar) * (m - v); out[i] = m
        return np.exp(out)
    if law == 'vu2_lin':                 # instant attack; release: damped second order on the linear gain
        w, z = p; x, vel = g[0], 0.0
        for i, v in enumerate(g):
            if v < x: x, vel = v, 0.0
            else:
                for _ in range(2):
                    h = DT / 2; vel += (w * w * (v - x) - 2 * z * w * vel) * h; x += vel * h
            out[i] = x
        return out
    if law == 'casc_lin':                # instant attack (both stages to the gain); release: two one-poles in cascade
        a1, a2 = np.exp(-DT / p[0]), np.exp(-DT / p[1]); x1 = x2 = g[0]
        for i, v in enumerate(g):
            if v < x2: x1 = x2 = v
            else: x1 = v + a1 * (x1 - v); x2 = x1 + a2 * (x2 - x1)
            out[i] = x2
        return out
    if law == 'vu2_db':
        w, z = p; lg = np.log(g); x, vel = lg[0], 0.0
        for i, v in enumerate(lg):
            if v < x: x, vel = v, 0.0
            else:
                for _ in range(2):
                    h = DT / 2; vel += (w * w * (v - x) - 2 * z * w * vel) * h; x += vel * h
            out[i] = x
        return np.exp(out)


def score(law, p, lag=0.0, detail=False):
    errs = []
    for tg, g, t, k in runs:
        m = sim(g, law, p)
        # the frame shows the meter as it was at (audio time - lag); the meter between frames is not seen
        pr = np.round(70 * np.interp(t - lag, tg, m))
        errs.append(k - pr)
    e = np.concatenate(errs)
    return (np.mean(e == 0), np.sqrt(np.mean(e ** 2)), np.mean(np.abs(e) <= 1)) if detail else np.sqrt(np.mean(e ** 2))


LAWS = (('vu2_lin', [15.7, 1.35]), ('casc_lin', [0.028, 0.143])) if '--final' in sys.argv else        (('exp_db', [0.1]), ('exp_lin', [0.1]), ('exp_db_att', [0.1, 0.002]), ('vu2_lin', [20.0, 0.8]), ('vu2_db', [20.0, 0.8]), ('casc_lin', [0.03, 0.14]))
for law, p0 in LAWS:
    best = None
    for lag in ((-0.01, -0.005, 0.0, 0.005) if '--final' in sys.argv else (0.0, 0.005, 0.01, 0.016)):
        r = minimize(lambda q: score(law, np.abs(q), lag), p0, method='Nelder-Mead', options=dict(xatol=1e-4, fatol=1e-3, maxiter=200))
        sc = score(law, np.abs(r.x), lag, True)
        if best is None or sc[1] < best[2][1]: best = (lag, np.abs(r.x), sc)
    lag, p, sc = best
    print('%-11s lag %.3f  params %s  exact %.1f%%  within 1: %.1f%%  rms %.2f steps' % (law, lag, np.round(p, 4), 100 * sc[0], 100 * sc[2], sc[1]), flush=True)

    if '--final' in sys.argv:
        for (tg, g, t, k), fn in zip(runs, [a for a in sys.argv[1:] if not a.startswith('--')]):
            pr = np.round(70 * np.interp(t - lag, tg, sim(g, law, p))); e = k - pr
            bad = np.nonzero(np.abs(e) > 1)[0]
            print('   %s: %d frames off by more than 1:' % (fn[-12:], len(bad)), ' '.join('%.2f(%+d)' % (t[i], e[i]) for i in bad[:30]))
