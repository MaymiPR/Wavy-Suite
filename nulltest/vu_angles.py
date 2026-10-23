"""Needle angle per frame from read_mc_gl.py output (MeterCapture gl=1 films): rays from the pivot, each scored by how red it is
(R - G above the face's, the logo's and the ticks') between two radii; the best ray, refined by a parabola. Angle in
degrees from the left horizontal, clockwise (0 = pointing left, 90 = straight up).

    python -I vu_angles.py <capture.npz> [<capture.npz> ...]       -> adds <capture>_angles.npy
"""
import sys
import numpy as np

PIVOT = (152.92, 170.36)          # crop pixels (x, y): the needle's pivot (pivot_fit.py; design 322.92, 220.36)
R0, R1 = 30, 88
ANG = np.arange(5.0, 175.0, 0.25)


def angles(meter, pivot=PIVOT):
    a = np.radians(ANG)[:, None]; r = np.arange(R0, R1, 1.0)[None, :]
    xs = np.clip(np.round(pivot[0] - r * np.cos(a)).astype(int), 0, meter.shape[2] - 1)
    ys = np.clip(np.round(pivot[1] - r * np.sin(a)).astype(int), 0, meter.shape[1] - 1)
    s = meter[:, ys, xs].astype(np.float32)                         # frames, angles, radii, rgb
    prof = np.mean(np.clip(s[..., 0] - s[..., 1] - 50.0, 0, None), 2)   # the dark red needle: R - G about 120
    i = np.clip(prof.argmax(1), 1, len(ANG) - 2)
    y0, y1, y2 = [prof[np.arange(len(i)), i + k] for k in (-1, 0, 1)]
    off = 0.5 * (y0 - y2) / np.where(np.abs(y0 - 2 * y1 + y2) < 1e-9, 1e-9, y0 - 2 * y1 + y2)
    return ANG[i] + np.clip(off, -1, 1) * (ANG[1] - ANG[0]), prof.max(1)


if __name__ == '__main__':
    for f in sys.argv[1:]:
        d = np.load(f)
        ang, conf = angles(d['meter'])
        np.save(f.replace('.npz', '_angles.npy'), np.stack([d['t'], ang, conf]))
        print(f, 'angles %.1f..%.1f, weakest score %.1f' % (ang.min(), ang.max(), conf.min()))
