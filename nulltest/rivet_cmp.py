"""H-Comp's needle tip and rivet against WComp's (needle_set_io.npz / needle_set_wc.npz): patches in needle coordinates
(s along, n across, 0.1 px), the median over strip positions 3 ... 33. Private helper."""
import numpy as np
from scipy import ndimage
from PIL import Image

P = np.array([153.42, 170.86])
DEG = [-58, -57, -55, -53, -52, -50, -48, -47, -45, -43, -42, -40, -38, -37, -35, -33, -32, -30, -28, -27, -25, -23, -22, -20,
       -18, -17, -15, -13, -12, -10, -8, -7, -5, -3, -2, 1, 2, 4, 6, 7, 9, 11, 12, 14, 16, 17, 19, 21, 22, 24, 26, 27, 29, 31]
S = np.arange(99, 114.01, 0.1); N = np.arange(-4, 4.01, 0.1); SS, NN = np.meshgrid(S, N, indexing='ij')


def patch(name):
    d = np.load('build/meters/needle_set_%s.npz' % name); fr = d['frames'].astype(np.float32); ks = list(d['k'])
    acc = []
    for k in range(3, 34):
        if k not in ks: continue
        th = np.radians(DEG[k]); u = np.array([np.sin(th), -np.cos(th)]); v = np.array([np.cos(th), np.sin(th)])
        Xs = P[0] + SS * u[0] + NN * v[0] - 0.5; Ys = P[1] + SS * u[1] + NN * v[1] - 0.5
        f = fr[ks.index(k)]
        acc.append(np.stack([ndimage.map_coordinates(f[..., c], [Ys, Xs], order=1) for c in range(3)], -1))
    return np.median(np.array(acc), 0)


h, w = patch('io'), patch('wc')
for lab, p in (('H', h), ('W', w)):
    L = p.sum(2); c = np.unravel_index(np.argmax(L), L.shape)
    print(lab, 'brightest at s %.1f n %.1f' % (S[c[0]], N[c[1]]), p[c].astype(int), ' | along n=0:',
          ' '.join('%.1f:%d' % (S[i], p[i, 40, 1]) for i in range(0, len(S), 5)))
print('mean abs diff in the patch: %.1f' % np.abs(h - w).mean())
img = np.concatenate([h, w, np.clip(128 + 2 * (w - h), 0, 255)], 1)
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((img.shape[1] * 4, img.shape[0] * 4), Image.NEAREST).save('build/meters/rivet_cmp.png')
