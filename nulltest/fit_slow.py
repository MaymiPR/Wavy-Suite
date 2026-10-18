import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import minimize
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'charge.npz'))
keys, vals = D['keys'], D['vals']          # keys: (base, L, d_ms); vals: x_det (dB) at +1.5,10,30,100,300,800 ms after the burst
base, L, dur = keys[:, 0].astype(float), keys[:, 1].astype(float), keys[:, 2].astype(float)
times = np.array([2, 10, 30, 100, 300, 800])
DT = 0.001
def simulate(p, model):
    aL, aE, tauE, relA, relB, relQ = p[:6]
    n_curve = len(base)
    s = base.copy()                                   # slow state in dB, starts at the base level
    t_end = dur + times.max() + 2
    out = np.zeros((n_curve, len(times)))
    T = int(max(t_end) / (DT*1000)) + 1
    for step in range(T):
        t = step * DT * 1000.0                        # ms
        f = np.where(t < dur, L, base)                # live level in dB (burst, then the base tone)
        gap_up = np.maximum(f - s, 0.0)
        up = aL * gap_up + aE * (10 ** (np.minimum(gap_up, 80) / 20.0) - 1.0) * 8.686 / tauE
        gap_dn = np.maximum(s - f, 0.0)
        dn = relA * gap_dn ** relQ / (1 + gap_dn / relB)
        s = s + (up - dn) * DT
        s = np.where(f > s, np.minimum(s, f), np.maximum(s, f))    # never cross the live level
        for k, tk in enumerate(times):
            m = np.abs(t - (dur + tk)) < DT*1000/2
            out[m, k] = np.maximum(s[m], f[m])
    return out
def err(p, verbose=False):
    if p[0] < 0 or p[1] < 0 or p[2] < 0.05 or p[3] < 0 or p[4] < 0.05 or p[5] < 0.2: return 1e3
    o = simulate(p, 0)
    e = o - vals
    if verbose: print('rms by time column', np.round(np.sqrt(np.mean(e**2, axis=0)), 2))
    return float(np.sqrt(np.mean(e**2)))
p0 = [5.5, 1.0, 1.3, 5.17, 0.5, 1.92]
print('start', err(p0, True))
best = None
for st in (p0, [3, 1.0, 1.0, 5, 0.5, 1.9], [8, 0.5, 2.0, 5, 0.5, 1.9], [0.5, 1.0, 0.3, 5, 0.5, 1.9]):
    r = minimize(err, st, method='Nelder-Mead', options={'maxiter': 500, 'xatol': 1e-3, 'fatol': 1e-3})
    print('->', np.round(r.x, 3), round(r.fun, 3), flush=True)
    if best is None or r.fun < best.fun: best = r
print('best', best.x, best.fun); err(best.x, True)
