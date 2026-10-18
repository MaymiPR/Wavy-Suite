import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import minimize
import fit_detector as F
D, names, DT = F.D, F.names, F.DT
def sim(e, p):
    a, b, q, m, tau_f = p
    rf = np.exp(-DT/max(tau_f, 1e-5))
    f = 0.0; s = 0.0; out = np.empty_like(e)
    for i, v in enumerate(e):
        f = max(v, f*rf)
        if f > s: s = f
        else:
            gap = min(20*np.log10(s/max(f, 1e-9)), 90.0)
            rate = a*gap**q/(1 + (gap/b)**1.0)*(s/0.1)**m
            s = max(f, s*10**(-rate*DT/20))
        out[i] = s
    return out
def err(p, verbose=False):
    a, b, q, m, tau_f = p
    if a <= 0 or b <= 1 or tau_f <= 0 or not (0.3 < q < 3): return 1e3
    tot = 0; n = 0
    for k in names:
        e = D[k+'_e']; xd = D[k+'_x']
        mdl = sim(e, p)
        sl = slice(int(0.3/F.DT), None)
        d = 20*np.log10(np.maximum(mdl[sl], 1e-9)/xd[sl]); d = d[e[sl] > 3e-3]
        if d.size == 0: continue
        if verbose: print('%-18s rms %.2f dB  mean %+.2f' % (k, np.sqrt(np.mean(d**2)), d.mean()))
        tot += np.sum(d**2); n += d.size
    return np.sqrt(tot/n)
best = None
for p0 in ([2.6, 34, 1.0, 0.0, 0.001], [3, 30, 0.8, 0.3, 0.005], [4, 20, 1.2, 0.0, 0.02], [2, 40, 1.0, -0.3, 0.001]):
    r = minimize(err, p0, method='Nelder-Mead', options={'xatol': 1e-3, 'fatol': 1e-3, 'maxiter': 400})
    print('start', p0, '->', np.round(r.x, 4), round(r.fun, 3), flush=True)
    if best is None or r.fun < best.fun: best = r
print('best', best.x, best.fun)
err(best.x, True)
