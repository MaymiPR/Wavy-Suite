"""The VU needle's pivot from MeterCapture gl=1 frames: a robust line through the needle's pixels in each of many
frames, then the point nearest all lines. Private measurement helper.

    python -I pivot_fit.py <capture.npz> [...]
"""
import sys
import numpy as np

G0 = np.array([150.0, 168.0])                        # rough pivot (crop px)


def needle_line(f):
    R, G, B = f[..., 0], f[..., 1], f[..., 2]
    ys, xs = np.nonzero((R > 100) & (R < 160) & (G < 40) & (B < 30))
    P = np.c_[xs, ys].astype(float)
    r = np.hypot(*(P - G0).T)
    P = P[(r > 25) & (r < 85)]
    for _ in range(4):
        if len(P) < 25: return None
        mu = P.mean(0); _, s, vt = np.linalg.svd(P - mu, full_matrices=False)
        dist = np.abs((P - mu) @ np.array([-vt[0][1], vt[0][0]]))
        P = P[dist < 1.5]
    return (mu, vt[0]) if s[1] / s[0] < 0.08 else None


lines = []
for fn in sys.argv[1:]:
    m = np.load(fn)['meter'].astype(np.int16)
    for i in np.linspace(0, len(m) - 1, 200).astype(int):
        L = needle_line(m[i])
        if L: lines.append(L)
A, b = np.zeros((2, 2)), np.zeros(2)
for mu, dv in lines:
    n = np.array([-dv[1], dv[0]]); A += np.outer(n, n); b += np.outer(n, n) @ mu
p = np.linalg.solve(A, b)
res = np.array([abs(np.array([-dv[1], dv[0]]) @ (p - mu)) for mu, dv in lines])
ang = np.degrees([np.arctan2(dv[1], dv[0]) % np.pi for mu, dv in lines])
print('lines %d (angles %.0f..%.0f), pivot crop (%.2f, %.2f), design (%.2f, %.2f), residual median %.2f px, max %.2f'
      % (len(lines), ang.min(), ang.max(), p[0], p[1], p[0] + 170, p[1] + 50, np.median(res), res.max()))
