import numpy as np, os, sys
from scipy.optimize import minimize
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'slow_dc.npz')
D = np.load(P); FS = 48000
keys = [tuple(k) for k in D['keys']]
times = np.array([2, 5, 10, 20, 35, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1000, 1200])
base = -40.0
ref = np.array([[D[f'{L}_{d}'][int(0.5*FS) + int(d*48) + int(t*48)] for t in times] for (L, d) in keys])
Ls = np.array([k[0] for k in keys], float); ds = np.array([k[1] for k in keys], float)
DT = 0.25
def sim(p):
    t1u, t1d, h, ta, tr = p
    k = lambda tau: 1 - np.exp(-DT/tau)
    k1u, k1d, ka, kr = k(t1u), k(t1d), k(ta), k(tr)
    t1 = np.full(len(keys), base); s = np.full(len(keys), base - h)
    out = np.zeros((len(keys), len(times)))
    T = int((ds.max() + 1250)/DT)
    for i in range(T):
        t = i*DT
        v = np.where(t < ds, Ls, base)
        t1 = t1 + (v - t1)*np.where(v > t1, k1u, k1d)
        up = t1 - h - s; dn = t1 + h - s
        s = s + np.where(up > 0, up*ka, np.where(dn < 0, dn*kr, 0.0))
        for j, tm in enumerate(times):
            m = np.abs(t - (ds + tm)) < DT/2
            out[m, j] = np.maximum(base, s[m])
    return out
def err(p):
    if min(p[:2]) < 0.05 or p[2] < 0 or min(p[3:]) < 1 or max(p) > 5000: return 1e3
    return float(np.sqrt(np.mean((sim(p) - ref)**2)))
best = None
for st in ([25, 25, 5, 180, 600], [10, 50, 3, 180, 500], [40, 10, 6, 150, 700], [5, 5, 1, 180, 600]):
    r = minimize(err, st, method='Nelder-Mead', options={'maxiter': 700, 'xatol': 1e-3, 'fatol': 1e-4})
    print(np.round(st, 1), '->', np.round(r.x, 3), round(r.fun, 4), flush=True)
    if best is None or r.fun < best.fun: best = r
print('best t1u t1d h ta tr:', np.round(best.x, 3), 'rms %.4f dB' % best.fun)
