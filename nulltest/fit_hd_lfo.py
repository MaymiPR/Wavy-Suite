"""Fits H-Delay's modulation (Phase 3): the LFO moves the target time T = D0 (1 + k depth/100 shape(phase)), and the
delay follows T with the glide law (d' = (T - d) / T, at most -1). Data: build/hd_mod.npy from the 500 Hz phase probe
(delay in ms every 1 ms, left then right, for depth/rate 100/1, 50/1, 25/1, 100/0.25, 100/4; Delay 100 ms)."""
import numpy as np
from scipy import optimize

CASES = [(100, 1.0), (50, 1.0), (25, 1.0), (100, 0.25), (100, 4.0)]
M = np.load('build/hd_mod.npy')
n = M.shape[1] // 2
fs = 1000.0                                  # the data's rate (1 ms)
SUB = 48                                     # glide simulated per audio sample


def shape(ph, kind):
    ph = ph % 1.0
    if kind == 'tri': return 1.0 - 4.0 * np.abs(ph - 0.5)        # -1 at 0, +1 at 0.5
    return -np.cos(2 * np.pi * ph)


def sim(p, depth, rate, kind, N):
    k, ph0, lag = p
    t = np.arange(N * SUB) / (fs * SUB)
    T = 100.0 * (1.0 + k * depth / 100.0 * shape(rate * (t - lag) + ph0, kind))
    d = np.empty_like(T); x = 100.0
    for i in range(len(T)):
        d[i] = x
        x += max(-1.0, (T[i] - x) / T[i]) / SUB * 1.0        # ms per ms
    return d[::SUB]


def err(p, kind, cases=CASES):
    e = 0.0
    for c, (depth, rate) in enumerate(cases):
        if (depth, rate) not in cases: continue
        meas = M[CASES.index((depth, rate)), :n]
        s = sim(p, depth, rate, kind, n)
        seg = slice(1500, n - 200)
        e += ((s[seg] - meas[seg]) ** 2).mean()
    return e


for kind in ('tri', 'sin'):
    best = None
    for ph0 in np.linspace(0, 1, 8, endpoint=False):
        r = optimize.minimize(err, [0.5, ph0, 0.0], args=(kind, [(100, 1.0)]), method='Nelder-Mead',
                              options={'xatol': 1e-4, 'fatol': 1e-6, 'maxiter': 400})
        if best is None or r.fun < best.fun: best = r
    print(kind, 'fit on 100/1: k %.4f phase %.4f lag %.4f rms %.3f ms' % (best.x[0], best.x[1], best.x[2], np.sqrt(best.fun)))
    for depth, rate in CASES:
        meas = M[CASES.index((depth, rate)), :n]
        # the phase at the start depends on when the LFO started: refit the phase per case, k fixed
        r = optimize.minimize(lambda q: err([best.x[0], q[0], 0.0], kind, [(depth, rate)]), [best.x[1]], method='Nelder-Mead')
        print('   %3d / %.2f Hz: rms %.3f ms (phase %.3f)' % (depth, rate, np.sqrt(r.fun), r.x[0]))

print('per case, triangle, k and phase free:')
for depth, rate in CASES:
    best = None
    for k0 in (0.3, 0.5, 0.7):
        r = optimize.minimize(lambda q: err([q[0], q[1], 0.0], 'tri', [(depth, rate)]), [k0 * 100 / depth * 0.5, 1.25 if rate != 0.25 else 1.0],
                              method='Nelder-Mead', options={'xatol': 1e-5, 'fatol': 1e-7})
        if best is None or r.fun < best.fun: best = r
    print('   %3d / %.2f Hz: k*depth/100 = %.4f (k %.4f), phase %.3f, rms %.3f ms' % (depth, rate, best.x[0] * depth / 100, best.x[0], best.x[1], np.sqrt(best.fun)))
