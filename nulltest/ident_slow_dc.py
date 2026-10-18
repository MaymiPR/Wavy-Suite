"""Compressor slow path on a DC carrier: level steps of various lengths (private)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; c = -24; M = 10**(-c/20)
def inv_law(g):           # detected amplitude for a gain g (linear, incl. 0.99)
    return (0.99*M/g - 1)/(M - 1)
base = -40
res = {}
for L in (-10, -20, -30):
    for d in (2, 5, 10, 20, 50, 100, 200, 400, 1000):
        x = np.concatenate([np.full(int(0.5*FS), 10**(base/20)), np.full(int(d/1000*FS), 10**(L/20)), np.full(int(1.6*FS), 10**(base/20))]).astype(np.float32)
        y = R.render(x, FS, comp=c)[0]
        g = np.abs(y/x)
        lev = 20*np.log10(np.maximum(inv_law(g), 1e-9))
        res[(L, d)] = lev
    print(L, flush=True)
np.savez(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'slow_dc.npz'),
         keys=np.array(list(res.keys())), **{f'{k[0]}_{k[1]}': v for k, v in res.items()})
s0 = int(0.5*FS)
for (L, d), lev in res.items():
    e = s0 + int(d/1000*FS)
    print('L %d d %4d ms: during(end-1) %.2f | after:' % (L, d, lev[e-1]), [(ms, round(float(lev[e + int(ms*48)]), 2)) for ms in (2, 5, 10, 20, 50, 100, 200, 400, 800, 1200)])
