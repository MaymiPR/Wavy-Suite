"""Cross-section of H-Comp's needle and its shadow (MeterCapture gl=1 films, needle_set.npz): each steady frame against a clean dial
(the 85th percentile of 124 frames with the needle at all 71 positions), in needle coordinates about the measured
pivot: s along the needle, n across it (+ to the needle's right). Private measurement helper.

    python -I needle_profile.py [k ...]
"""
import sys
import numpy as np
from scipy import ndimage

P = np.array([153.42, 170.86])                       # crop, continuous (pixel centres at +0.5): design 323.42, 220.86
d = np.load('../build/meters/needle_set.npz'); fr = d['frames'].astype(np.float32); ks = d['k']; bg = d['bg']


def profile(k, s0=40, s1=95, nr=np.arange(-6, 12.01, 0.25)):
    f = fr[list(ks).index(k)]
    th = np.radians(-58.25 + k * 117.17 / 70)
    u = np.array([np.sin(th), -np.cos(th)]); v = np.array([np.cos(th), np.sin(th)])
    ss = np.arange(s0, s1, 0.5)
    S, N = np.meshgrid(ss, nr, indexing='ij')
    X = P[0] + S * u[0] + N * v[0] - 0.5; Y = P[1] + S * u[1] + N * v[1] - 0.5   # pixel centres
    img = np.stack([ndimage.map_coordinates(f[..., c], [Y, X], order=1) for c in range(3)], -1)
    ref = np.stack([ndimage.map_coordinates(bg[..., c], [Y, X], order=1) for c in range(3)], -1)
    return nr, np.median(img, 0), np.median(ref, 0), np.median(img / np.maximum(ref, 1), 0)


if __name__ == '__main__':
    for k in [int(a) for a in sys.argv[1:]] or [5, 20, 35, 50, 65]:
        nr, img, ref, ratio = profile(k)
        print('k %d (angle %.1f): n: R G B / ratio G' % (k, -58.25 + k * 117.17 / 70))
        print('  ' + ' '.join('%+.2f:%d,%d,%d/%.2f' % (n, *img[i].astype(int), ratio[i, 1]) for i, n in enumerate(nr) if abs(n * 4) % 2 == 0))
