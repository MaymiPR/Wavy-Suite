import numpy as np, os
from scipy.signal import lfilter
from scipy.optimize import least_squares, minimize
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'kernel_patterns.npz'))
pats, lens, read = D['pats'], D['lens'], D['read']
X = np.zeros((len(pats), 400)); X[:, 4:12] = pats
def ap2(a1, a2): return [a2, a1, 1.0], [1.0, a1, a2]
def pred(par, order=2):
    if order == 1:
        a = par[0]; b, aa = [a, 1.0], [1.0, a]
        Y = lfilter(b, aa, X, axis=1)
    else:
        b, aa = ap2(par[0], par[1]); Y = lfilter(b, aa, X, axis=1)
    return np.maximum(np.abs(X).max(axis=1), np.abs(Y).max(axis=1))
def res(par, order=2): return pred(par, order) - read
best = None
for order, starts in ((1, [[v] for v in (-0.9, -0.5, 0.5, 0.9)]), (2, [[a, b] for a in (-1.5, -0.5, 0.5, 1.5) for b in (0.2, 0.6, 0.9)])):
    for s in starts:
        try:
            r = least_squares(res, s, args=(order,), diff_step=1e-3)
        except Exception as e:
            continue
        rms = np.sqrt(np.mean(r.fun ** 2))
        if best is None or rms < best[0]: best = (rms, order, r.x)
        print(order, np.round(s, 2), '->', np.round(r.x, 4), 'rms %.4f' % rms)
print('best', best)

print('--- more structures')
def cascade(par):
    Y = X
    for a in par: Y = lfilter([a, 1.0], [1.0, a], Y, axis=1)
    return Y
def res_c(par): 
    Y = cascade(par); return np.maximum(np.abs(X).max(axis=1), np.abs(Y).max(axis=1)) - read
for st in ([0.48, 0.2], [0.48, 0.7], [0.3, 0.6], [0.5, -0.3], [0.48, 0.48, 0.48]):
    r = least_squares(res_c, st, diff_step=1e-3); print('cascade', np.round(st, 2), '->', np.round(r.x, 4), 'rms %.4f' % np.sqrt(np.mean(r.fun ** 2)))
def res_two(par):          # max of |x|, |H(a) x| and |H(b) x|
    Y1 = lfilter([par[0], 1.0], [1.0, par[0]], X, axis=1); Y2 = lfilter([par[1], 1.0], [1.0, par[1]], X, axis=1)
    return np.maximum(np.abs(X).max(axis=1), np.maximum(np.abs(Y1).max(axis=1), np.abs(Y2).max(axis=1))) - read
for st in ([0.48, -0.48], [0.48, 0.8], [0.3, 0.7]):
    r = least_squares(res_two, st, diff_step=1e-3); print('max of two allpasses', np.round(st, 2), '->', np.round(r.x, 4), 'rms %.4f' % np.sqrt(np.mean(r.fun ** 2)))
r1 = res([0.4835], 1); idx = np.argsort(-np.abs(r1))[:10]
print('worst patterns for the single allpass:')
for i in idx: print(np.round(pats[i][:lens[i]], 2), 'obs %.3f pred %.3f' % (read[i], read[i] + r1[i]))
