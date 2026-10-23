"""Fits H-Comp's needle shadow on the filmed frames (needle_set.npz from the per-position frames): the dial's
darkening (1 - frame / clean dial, green channel) outside the red needle, modelled as a Gaussian ridge of depth A and
width sigma around the needle's segment from radius s0 to s1, translated by (dx, dy). Private measurement helper."""
import numpy as np
from scipy.optimize import least_squares

P = np.array([153.42, 170.86])                      # crop, continuous (pixel centres at +0.5): design 323.42, 220.86
d = np.load('../build/meters/needle_set_gr.npz'); fr = d['frames'].astype(np.float32); ks = d['k']; bg = d['bg']
H, W, _ = bg.shape
yy, xx = np.mgrid[0:H, 0:W]; X = xx + 0.5; Y = yy + 0.5
rr = np.hypot(X - P[0], Y - P[1])
data = []
for f, k in zip(fr, ks):
    if k in (0, 70) or k % 3: continue
    th = np.radians(-58.25 + k * 117.17 / 70)
    dark = 1 - f[..., 1] / np.maximum(bg[..., 1], 1)
    red = (f[..., 0] - f[..., 1]) > 25
    from scipy import ndimage
    red = ndimage.binary_dilation(red, iterations=1)
    m = (~red) & (rr > 28) & (rr < 120) & (Y < 158) & (bg[..., 1] > 120)        # the cream face only
    data.append((th, X[m], Y[m], dark[m]))


def model(p, th, x, y):
    dx, dy, A, sig, s0, s1, c = p
    u = np.array([np.sin(th), -np.cos(th)])
    qx, qy = x - (P[0] + dx), y - (P[1] + dy)
    s = qx * u[0] + qy * u[1]
    n = -qx * u[1] + qy * u[0]
    sc = np.clip(s, s0, s1)
    dist2 = (s - sc) ** 2 + n ** 2
    return A * np.exp(-dist2 / (2 * sig * sig)) + c


def res(p):
    return np.concatenate([model(p, th, x, y) - v for th, x, y, v in data])


r = least_squares(res, [0.0, 5.0, 0.25, 1.0, 25.0, 105.0, 0.0], loss='soft_l1', f_scale=0.05)
dx, dy, A, sig, s0, s1, c = r.x
print('shadow offset (%.2f, %.2f) px, depth %.3f, sigma %.2f px (FWHM %.2f), from r %.1f to %.1f, floor %.3f; frames %d'
      % (dx, dy, A, sig, 2.355 * sig, s0, s1, c, len(data)))
# colour of the shadow: per-channel ratio at the ridge
rat = []
for f, k in zip(fr, ks):
    if k in (0, 70) or k % 3: continue
    th = np.radians(-58.25 + k * 117.17 / 70); u = np.array([np.sin(th), -np.cos(th)])
    for s in np.arange(45, 95, 5.0):
        x, y = P[0] + dx + s * u[0], P[1] + dy + s * u[1]
        xi, yi = int(x), int(y)
        if abs(u[0]) < 0.35 or not (0 <= yi < H and 0 <= xi < W) or (f[yi, xi, 0] - f[yi, xi, 1]) > 25: continue
        rat.append(f[yi, xi] / np.maximum(bg[yi, xi], 1))
rat = np.array(rat); print('ridge colour ratio R G B: %.3f %.3f %.3f (n %d)' % (*np.median(rat, 0), len(rat)))
