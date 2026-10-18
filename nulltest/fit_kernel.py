import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import least_squares
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'kernel_patterns.npz'))
pats, lens, read = D['pats'], D['lens'], D['read']
RES = 8; HALF = 6                                    # kernel support +-6 samples at 1/8 resolution
offs = np.arange(0, HALF*RES + 1) / RES              # 0 .. 6
nonint = [i for i, o in enumerate(offs) if abs(o - round(o)) > 1e-9]
def kernel_from(par):
    h = np.zeros(len(offs)); h[0] = 1.0
    for i, v in zip(nonint, par): h[i] = v
    return h
def hval(h, d):
    a = np.abs(d)
    idx = np.rint(a * RES).astype(int)
    out = np.where(idx <= HALF*RES, h[np.clip(idx, 0, HALF*RES)], 0.0)
    return out
def predict(par, p, L, p_norm=60.0):
    h = kernel_from(par)
    t = np.arange(-2, L + 2 + 1e-9, 1/RES)
    y = np.zeros_like(t)
    for m in range(L): y += p[m] * hval(h, t - m)
    a = np.abs(y); mx = a.max()
    return mx * (np.sum((a/mx)**p_norm))**(1/p_norm) / (len(a)**(1/p_norm)) * 1.0 if False else mx
def resid(par):
    return np.array([predict(par, pats[i], lens[i]) - read[i] for i in range(len(pats))])
# start: ideal sinc with a Hann window
sinc0 = np.sinc(offs) * (0.5 + 0.5*np.cos(np.pi*offs/(HALF+0.5)))
x0 = np.array([sinc0[i] for i in nonint])
print('start rms', np.sqrt(np.mean(resid(x0)**2)))
r = least_squares(resid, x0, method='trf', max_nfev=60, diff_step=1e-3)
print('fit rms', np.sqrt(np.mean(r.fun**2)), 'max', np.abs(r.fun).max())
h = kernel_from(r.x)
np.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'kernel_h.npy'), h)
print('kernel at 0.25 steps:'); print(np.round(h[::2][:25], 4))
