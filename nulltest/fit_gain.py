import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import minimize
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'charge.npz'))
keys, vals = D['keys'], D['vals']
base, L, dur = keys[:, 0].astype(float), keys[:, 1].astype(float), keys[:, 2].astype(float)
times = np.array([2, 10, 30, 100, 300, 800]); c = -24; M = 10**(-c/20); DT = 0.001
def law(x): return 0.99*M/(1 + (M-1)*x)
G = law(10**(vals/20))                      # reference gain (linear) at the read-out times
def simulate(p, mode):
    ta1, tr1, ta2, tr2 = p
    gt_b = law(10**(base/20)); gt_L = law(10**(L/20))
    s1 = gt_b.copy(); s2 = gt_b.copy()
    T = int((dur.max() + times.max() + 2)/ (DT*1000)) + 1
    out = np.zeros((len(base), len(times)))
    for step in range(T):
        t = step*DT*1000
        gt = np.where(t < dur, gt_L, gt_b)
        a1 = np.where(gt < s1, DT/ta1, DT/tr1)
        s1 = s1 + (gt - s1)*np.minimum(a1, 1)
        if mode == 2:
            a2 = np.where(s1 < s2, DT/ta2, DT/tr2)
            s2 = s2 + (s1 - s2)*np.minimum(a2, 1)
            sm = s2
        else:
            sm = s1
        g = np.minimum(gt, sm)
        for k, tk in enumerate(times):
            m = np.abs(t - (dur + tk)) < DT*1000/2
            out[m, k] = g[m]
    return out
def err(p, mode, verbose=False):
    if min(p) < 0.005: return 1e3
    o = simulate(p, mode)
    e = 20*np.log10(o/G)
    if verbose: print('rms by column', np.round(np.sqrt(np.mean(e**2, axis=0)), 2))
    return float(np.sqrt(np.mean(e**2)))
for mode, starts in ((1, [[0.1, 0.45, 1, 1], [0.2, 0.3, 1, 1]]), (2, [[0.075, 0.45, 0.075, 0.45], [0.05, 0.3, 0.1, 0.6], [0.04, 0.4, 0.04, 0.4]])):
    best = None
    for st in starts:
        r = minimize(err, st, args=(mode,), method='Nelder-Mead', options={'maxiter': 400, 'xatol': 1e-4, 'fatol': 1e-4})
        print('mode', mode, np.round(st, 3), '->', np.round(r.x, 4), round(r.fun, 3), flush=True)
        if best is None or r.fun < best.fun: best = r
    print('best mode', mode, best.x, best.fun); err(best.x, mode, True)
