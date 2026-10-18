import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import minimize
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'charge.npz'))
keys, vals = D['keys'], D['vals']
base, L, dur = keys[:, 0].astype(float), keys[:, 1].astype(float), keys[:, 2].astype(float)
times = np.array([2, 10, 30, 100, 300, 800]); c = -24; M = 10**(-c/20); DT = 0.001
def law(x): return 0.99*M/(1 + (M-1)*x)
G = law(10**(vals/20))
def simulate(p, domain):
    ta1, tr1, ta2, tr2 = p
    fb = 10**(base/20); fL = 10**(L/20)
    s1 = fb.copy(); s2 = fb.copy()
    out = np.zeros((len(base), len(times)))
    T = int((dur.max() + times.max() + 2)/(DT*1000)) + 1
    for step in range(T):
        t = step*DT*1000
        f = np.where(t < dur, fL, fb)
        if domain == 'amp':
            s1 = s1 + (f - s1)*np.minimum(np.where(f > s1, DT/ta1, DT/tr1), 1)
            s2 = s2 + (s1 - s2)*np.minimum(np.where(s1 > s2, DT/ta2, DT/tr2), 1)
            lev = np.maximum(f, s2)
        else:   # dB
            fd = 20*np.log10(f); sd1 = 20*np.log10(s1); sd2 = 20*np.log10(s2)
            sd1 = sd1 + (fd - sd1)*np.minimum(np.where(fd > sd1, DT/ta1, DT/tr1), 1)
            sd2 = sd2 + (sd1 - sd2)*np.minimum(np.where(sd1 > sd2, DT/ta2, DT/tr2), 1)
            s1 = 10**(sd1/20); s2 = 10**(sd2/20)
            lev = np.maximum(f, s2)
        g = law(lev)
        for k, tk in enumerate(times):
            m = np.abs(t - (dur + tk)) < DT*1000/2
            out[m, k] = g[m]
    return out
def err(p, domain):
    if min(p) < 0.005: return 1e3
    o = simulate(p, domain); e = 20*np.log10(o/G)
    return float(np.sqrt(np.mean(e**2)))
for domain in ('amp', 'db'):
    best = None
    for st in ([0.1, 0.4, 0.1, 0.4], [0.3, 0.1, 0.3, 0.5], [0.05, 0.02, 0.1, 0.45]):
        r = minimize(err, st, args=(domain,), method='Nelder-Mead', options={'maxiter': 300, 'xatol': 1e-4, 'fatol': 1e-4})
        if best is None or r.fun < best.fun: best = r
    print(domain, np.round(best.x, 4), round(best.fun, 3), flush=True)
