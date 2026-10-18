import numpy as np, os
from scipy.optimize import minimize
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'slow_dc.npz')
D = np.load(P); FS = 48000
keys = [tuple(k) for k in D['keys']]
times = np.array([2, 5, 10, 20, 50, 100, 200, 400, 800, 1200])
base = -40
ref = np.array([[D[f'{L}_{d}'][int(0.5*FS) + int(d*48) + int(t*48)] for t in times] for (L, d) in keys])
Ls = np.array([k[0] for k in keys], float); ds = np.array([k[1] for k in keys], float)
DT = 0.5   # ms
def sim(p, dom):
    a1, r1, a2, r2 = p
    k = lambda tau: 1 - np.exp(-DT/tau)
    ka1, kr1, ka2, kr2 = k(a1), k(r1), k(a2), k(r2)
    conv = (lambda db: 10**(db/20)) if dom == 'amp' else (lambda db: db)
    back = (lambda v: 20*np.log10(np.maximum(v, 1e-9))) if dom == 'amp' else (lambda v: v)
    s1 = np.full(len(keys), conv(base)); s2 = s1.copy()
    out = np.zeros((len(keys), len(times)))
    T = int((ds.max() + 1300)/DT)
    for i in range(T):
        t = i*DT
        v = conv(np.where(t < ds, Ls, base))
        s1 = s1 + (v - s1)*np.where(v > s1, ka1, kr1)
        s2 = s2 + (s1 - s2)*np.where(s1 > s2, ka2, kr2)
        for j, tm in enumerate(times):
            m = np.abs(t - (ds + tm)) < DT/2
            out[m, j] = back(s2[m])
    return out
def err(p, dom):
    if min(p) < 0.5 or max(p) > 5000: return 1e3
    return float(np.sqrt(np.mean((sim(p, dom) - ref)**2)))
for dom in ('amp', 'db'):
    best = None
    for st in ([100, 150, 100, 450], [200, 300, 50, 300], [50, 100, 300, 500], [150, 150, 150, 150]):
        r = minimize(err, st, args=(dom,), method='Nelder-Mead', options={'maxiter': 600, 'xatol': 1e-3, 'fatol': 1e-4})
        if best is None or r.fun < best.fun: best = r
    print(dom, 'attack1 release1 attack2 release2 (ms):', np.round(best.x, 2), 'rms %.3f dB' % best.fun, flush=True)
