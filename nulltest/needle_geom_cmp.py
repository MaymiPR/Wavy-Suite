"""Line fit and cross profile of a needle, H-Comp's films (needle_set_io.npz) against WComp's render (VuRender), per
strip position: angle, lateral offset from the pivot, darkness profile across the needle. Private helper."""
import sys, os, subprocess
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compare_needle import render, DEG  # noqa: E402  (renders with argv face)

P = np.array([153.42, 170.86])
d = np.load('build/meters/needle_set_io.npz'); fr = d['frames'].astype(np.float32); ks = list(d['k']); bg = d['bg']
bgW = render(150)[..., :3]
H, W = bg.shape[:2]
yy, xx = np.mgrid[0:H, 0:W]; X = xx + 0.5; Y = yy + 0.5; rr = np.hypot(X - P[0], Y - P[1])


def fit(f, b):
    dark = np.clip((b[..., 1] - f[..., 1]) / np.maximum(b[..., 1], 1), 0, 1)
    m = (rr > 30) & (rr < 98) & (Y < P[1] - 8) & ((f[..., 0] - f[..., 1]) > 60)
    w = dark[m]; x = X[m]; y = Y[m]
    mu = np.array([np.average(x, weights=w), np.average(y, weights=w)])
    C = np.cov(np.c_[x - mu[0], y - mu[1]].T, aweights=w); u = np.linalg.eigh(C)[1][:, 1]
    if u[1] > 0: u = -u
    nrm = np.array([-u[1], u[0]])
    q = np.c_[X.ravel() - P[0], Y.ravel() - P[1]]; s = q @ u; n = q @ nrm
    sel = (s > 45) & (s < 90) & (np.abs(n) < 3)
    g = (f[..., 1] / np.maximum(b[..., 1], 1)).ravel()[sel]
    bins = np.arange(-2.5, 2.51, 0.25); prof = [np.median(g[(n[sel] >= a) & (n[sel] < a + 0.25)]) for a in bins[:-1]]
    width = np.sum(1 - np.array(prof)) * 0.25                 # equivalent width of full darkness
    return np.degrees(np.arctan2(u[0], -u[1])), (P - mu) @ nrm, width, prof


for k in (4, 14, 24, 45, 55, 66):
    aH, oH, wH, pH = fit(fr[ks.index(k)], bg)
    w = render(DEG[k])[..., :3]
    aW, oW, wW, pW = fit(w, bgW)
    print('k %2d: angle H %+.2f W %+.2f | pivot off line H %+.2f W %+.2f px | equiv width H %.2f W %.2f px' % (k, aH, aW, oH, oW, wH, wW))
print('profile H', ' '.join('%.2f' % v for v in pH)); print('profile W', ' '.join('%.2f' % v for v in pW))
