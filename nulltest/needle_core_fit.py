"""H-Comp's needle line per strip position (needle_set.npz): a weighted line fit on the red needle (weight = redness),
its angle against the uniform strip law, its distance from the pivot, and the colour across it. Private helper."""
import numpy as np
from scipy import ndimage

P = np.array([153.42, 170.86])                      # crop, continuous (pixel centres at +0.5): design 323.42, 220.86
d = np.load('../build/meters/needle_set_gr.npz'); fr = d['frames'].astype(np.float32); ks = d['k']; bg = d['bg']
H, W, _ = bg.shape
yy, xx = np.mgrid[0:H, 0:W]; X = xx + 0.5; Y = yy + 0.5
rr = np.hypot(X - P[0], Y - P[1])
out = []
prof = []
for f, k in sorted(zip(fr, ks), key=lambda q: q[1]):
    red = np.clip((bg[..., 1] - f[..., 1]) / np.maximum(bg[..., 1], 1), 0, 1)       # 1 at pure needle red
    m = (rr > 30) & (rr < 98) & (Y < P[1] - 8) & ((f[..., 0] - f[..., 1]) > 60)
    w = red[m]; x = X[m]; y = Y[m]
    mu = np.array([np.average(x, weights=w), np.average(y, weights=w)])
    C = np.cov(np.c_[x - mu[0], y - mu[1]].T, aweights=w)
    ev, evec = np.linalg.eigh(C); u = evec[:, 1]
    if u[1] > 0: u = -u                               # pointing up
    ang = np.degrees(np.arctan2(u[0], -u[1]))
    nrm = np.array([-u[1], u[0]])
    off = (P - mu) @ nrm                              # the pivot's distance from the line
    out.append((k, ang, ang - (-58.25 + k * 117.17 / 70), off))
    # cross profile of the green channel ratio about the fitted line (s 40..90)
    q = np.c_[X.ravel() - mu[0], Y.ravel() - mu[1]]
    s = q @ u; n = q @ nrm
    sel = ((s + (mu - P) @ u) > 40) & ((s + (mu - P) @ u) < 90) & (np.abs(n) < 3)
    g = (f[..., 1] / np.maximum(bg[..., 1], 1)).ravel()[sel]; rch = (f[..., 0] / np.maximum(bg[..., 0], 1)).ravel()[sel]
    prof.append((n[sel], g, rch))
o = np.array(out)
print('angle error vs strip law: mean %+.3f, sd %.3f, max %.3f deg; pivot off the line: mean %+.3f, sd %.3f px' % (o[:, 2].mean(), o[:, 2].std(), abs(o[:, 2]).max(), o[:, 3].mean(), o[:, 3].std()))
A = np.c_[np.ones(len(o)), o[:, 0]]; c = np.linalg.lstsq(A, o[:, 1], rcond=None)[0]
print('best uniform law: %.3f + k * %.5f (law: -58.25 + k * 1.67386); residual sd %.3f' % (c[0], c[1], (o[:, 1] - A @ c).std()))
n = np.concatenate([p[0] for p in prof]); g = np.concatenate([p[1] for p in prof]); r = np.concatenate([p[2] for p in prof])
bins = np.arange(-3, 3.01, 0.25)
print('across the needle (n px: green ratio / red ratio):')
print('  ' + ' '.join('%+.2f:%.2f/%.2f' % (b + 0.125, np.median(g[(n >= b) & (n < b + 0.25)]), np.median(r[(n >= b) & (n < b + 0.25)])) for b in bins[:-1]))
