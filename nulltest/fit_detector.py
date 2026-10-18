"""Fit the detector (fast follower + slow state) to trajectories measured on the reference (private)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
from scipy.optimize import minimize
FS = 48000; c = -24; W = 16; DT = W/FS
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'det_traj.npz')
def xdet_from_gain(g_db, c):
    M = 10**(-c/20); g = 10**(g_db/20); return (0.99*M/g - 1)/(M - 1)
def traj(x, c):
    y = R.render(x.astype(np.float32), FS, comp=c, preroll=0.1)[0]
    e = lambda s: np.abs(s[:len(s)//W*W]).reshape(-1, W).max(axis=1)
    ei, eo = e(x), e(y)
    g = 20*np.log10(eo/np.maximum(ei, 1e-9))
    return ei, np.maximum(xdet_from_gain(g, c), 1e-7)
def make_cases():
    t = np.arange(int(3*FS))/FS
    C = {}
    for lo, hi in ((-10, -50), (-10, -30), (-10, -20), (-30, -50), (-20, -60)):
        C[f'step{lo}_{hi}'] = np.concatenate([R.tone(FS, 3000, 0.5, lo), R.tone(FS, 3000, 2.5, hi)])
    for fm, d in ((1, .5), (2, .5), (5, .5), (10, .9), (20, .9), (40, .9), (3, .95)):
        C[f'am{fm}_{d}'] = 0.1*(1+d*np.sin(2*np.pi*fm*t))*np.sin(2*np.pi*3000*t)
    # bursts: tone on/off train
    for on, off in ((0.05, 0.2), (0.02, 0.1)):
        per = int((on+off)*FS); x = np.zeros(int(3*FS))
        for s in range(0, len(x)-per, per): x[s:s+int(on*FS)] = 0.1*np.sin(2*np.pi*3000*np.arange(int(on*FS))/FS)
        C[f'burst{on}_{off}'] = x
    return C
if not os.path.exists(CACHE):
    out = {}
    for k, x in make_cases().items():
        e, xd = traj(x, c); out[k+'_e'] = e; out[k+'_x'] = xd
        print(k, flush=True)
    np.savez(CACHE, **out)
D = np.load(CACHE)
names = sorted({k[:-2] for k in D.files})
def sim(e, p):
    tau_f, a, b, floor = p
    rf = np.exp(-DT/tau_f)
    f = 0.0; s = 0.0; out = np.empty_like(e)
    for i, v in enumerate(e):
        f = max(v, f*rf)
        if f > s: s = f
        else:
            gap = 20*np.log10(s/max(f, 1e-9))
            gap = min(gap, 90.0)
            rate = a*gap/(1 + gap/b)                    # dB/s
            s = s*10**(-rate*DT/20)
            s = max(s, f)
        out[i] = s
    return out
def err(p, verbose=False):
    if p[0] <= 1e-4 or p[1] <= 0 or p[2] <= 1: return 1e3
    tot = 0; n = 0
    for k in names:
        e = D[k+'_e']; xd = D[k+'_x']
        m = sim(e, p)
        sl = slice(int(0.3/DT), None)
        d = 20*np.log10(np.maximum(m[sl], 1e-9)/xd[sl])
        d = d[e[sl] > 3e-3]            # the reference gain is only defined where there is signal
        if verbose: print('%-18s rms %.2f dB  mean %.2f' % (k, np.sqrt(np.mean(d**2)), d.mean()))
        tot += np.sum(d**2); n += d.size
    return np.sqrt(tot/n)
if __name__ == '__main__':
    p0 = [0.02, 2.2, 20.0, 0.0]
    print('start', err(p0, True))
    r = minimize(err, p0, method='Nelder-Mead', options={'xatol': 1e-3, 'fatol': 1e-3, 'maxiter': 300})
    print('fit', r.x, r.fun)
    err(r.x, True)
