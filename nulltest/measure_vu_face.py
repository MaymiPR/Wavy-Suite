"""Measures a VU face's print from MeterCapture gl=1 crops (needle removed: the median of frames with the needle in
three places), in polar coordinates about WComp's scale centre (design 320.7, 220.9; angle from vertical, + right):
the red band's extent and dividers, the arc's ticks, the dots and the numbers' centres. Private measurement helper.

    python -I measure_vu_face.py <capture.npz> <t1> <t2> <t3>
"""
import sys
import numpy as np
from scipy import ndimage

C = np.array([320.7, 220.9]); CROP0 = np.array([170.0, 50.0])
d = np.load(sys.argv[1]); m = d['meter'].astype(np.float32); t = d['t']
f = np.median(np.stack([m[np.argmin(abs(t - float(s)))] for s in sys.argv[2:5]]), 0)
H, W, _ = f.shape
yy, xx = np.mgrid[0:H, 0:W]
X = xx + CROP0[0] + 0.5 - 0.5; Y = yy + CROP0[1]
r = np.hypot(X - C[0], Y - C[1]); a = np.degrees(np.arctan2(X - C[0], -(Y - C[1])))
R, G, B = f[..., 0], f[..., 1], f[..., 2]
face = (r > 85) & (r < 126) & (Y < 205) & (abs(a) < 66)
red = face & (R > 190) & (G < 130) & (B < 110)
dark = face & (R < 110) & (G < 100) & (B < 90)

ar = a[red]; rr = r[red]
print('red band: angle %.2f .. %.2f, radius %.2f .. %.2f' % (np.percentile(ar, 0.5), np.percentile(ar, 99.5), np.percentile(rr, 1), np.percentile(rr, 99)))
# dividers: dark pixels inside the band's radius range and angle range
bandA = (np.percentile(ar, 0.5), np.percentile(ar, 99.5))
inb = dark & (r > 101.5) & (r < 105.5) & (a > bandA[0] + 0.8) & (a < bandA[1] - 0.8)
h, e = np.histogram(a[inb], bins=np.arange(-66, 66, 0.25))
pk = [0.5 * (e[i] + e[i + 1]) for i in range(1, len(h) - 1) if h[i] >= 3 and h[i] >= h[i - 1] and h[i] > h[i + 1]]
print('dividers:', ' '.join('%.1f' % v for v in pk))
# ticks: dark pixels just outside the arc, outside the band
tk = dark & (r > 101.8) & (r < 109) & ~((a > bandA[0] - 0.5) & (a < bandA[1] + 0.5))
lab, n = ndimage.label(tk)
ticks = []
for i in range(1, n + 1):
    s = lab == i
    if s.sum() < 3: continue
    ticks.append((a[s].mean(), r[s].max()))
print('ticks (angle, outer radius):', ' '.join('%.1f/%.1f' % q for q in sorted(ticks)))
# dots and numbers: dark components between radius 109 and 126
ob = dark & (r > 109) & (r < 126)
lab, n = ndimage.label(ob, structure=np.ones((3, 3)))
comps = []
for i in range(1, n + 1):
    s = lab == i
    if s.sum() < 4: continue
    w = (255 - f[..., 1])[s]
    comps.append((np.average(a[s], weights=w), np.average(r[s], weights=w), s.sum(), np.ptp(X[s]) + 1, np.ptp(Y[s]) + 1))
for q in sorted(comps): print('  component angle %6.2f radius %6.2f  px %3d  size %dx%d' % q)
