import numpy as np, os, itertools
from scipy.signal import lfilter
from scipy.optimize import least_squares
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'kernel_patterns.npz'))
pats, lens, read = D['pats'], D['lens'], D['read']
X = np.zeros((len(pats), 400)); X[:, 4:12] = pats
def chain(par, Y):
    for a in par: Y = lfilter([a, 1.0], [1.0, a], Y, axis=1)
    return Y
def make(n0, n1, with_x):
    def res(p):
        Y0 = chain(p[:n0], X); Y1 = chain(p[n0:n0 + n1], X)
        m = np.maximum(np.abs(Y0).max(axis=1), np.abs(Y1).max(axis=1))
        if with_x: m = np.maximum(m, np.abs(X).max(axis=1))
        return m - read
    return res
for (n0, n1) in ((1, 1), (2, 1), (1, 2), (2, 2)):
    for with_x in (False, True):
        best = None
        for _ in range(12):
            st = np.random.default_rng(len(str(best)) + _).uniform(0.05, 0.95, n0 + n1)
            try: r = least_squares(make(n0, n1, with_x), st, diff_step=1e-3, bounds=(-0.999, 0.999))
            except Exception: continue
            rms = np.sqrt(np.mean(r.fun ** 2))
            if best is None or rms < best[0]: best = (rms, r.x, np.abs(r.fun).max())
        print('branch0 %d sections, branch1 %d, with |x| %s: rms %.4f max %.3f  coefs %s' % (n0, n1, with_x, best[0], best[2], np.round(best[1], 4)))

print('--- k parallel first-order allpass branches (plus |x|)')
def make_k(k, neg=False):
    def res(p):
        m = np.abs(X).max(axis=1)
        for a in p: m = np.maximum(m, np.abs(lfilter([a, 1.0], [1.0, a], X, axis=1)).max(axis=1))
        return m - read
    return res
for k in (2, 3, 4):
    best = None
    for s in range(25):
        st = np.random.default_rng(100 + s).uniform(-0.9, 0.95, k)
        try: r = least_squares(make_k(k), st, diff_step=1e-3, bounds=(-0.999, 0.999))
        except Exception: continue
        rms = np.sqrt(np.mean(r.fun ** 2))
        if best is None or rms < best[0]: best = (rms, np.sort(r.x), np.abs(r.fun).max())
    print('k=%d  rms %.4f max %.3f coefs %s' % (k, best[0], best[2], np.round(best[1], 4)))
