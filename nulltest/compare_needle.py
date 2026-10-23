"""Pixel comparison of WComp's VU needle (tools/VuRender) with H-Comp's filmed needle at the same strip positions
(needle_set_<face>.npz): both dials with the needle, the difference around the needle and its shadow only (the
print's own differences cancel: each is measured against the same dial without its needle). Private helper.

    python -I compare_needle.py io|gr [k ...]   -> build/meters/cmp_needle_<face>.png
"""
import sys, os, subprocess
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'plugins', 'WComp'))
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
VR = os.path.join(ROOT, 'build', 'VuRender_artefacts', 'Release', 'VuRender.exe')
TMP = os.path.join(ROOT, 'build', 'meters', 'vr_tmp.png')
DEG = [-58, -57, -55, -53, -52, -50, -48, -47, -45, -43, -42, -40, -38, -37, -35, -33, -32, -30, -28, -27, -25, -23, -22, -20,
       -18, -17, -15, -13, -12, -10, -8, -7, -5, -3, -2, 1, 2, 4, 6, 7, 9, 11, 12, 14, 16, 17, 19, 21, 22, 24, 26, 27, 29, 31,
       32, 34, 36, 37, 39, 41, 42, 44, 46, 47, 49, 51, 52, 54, 56, 57, 59]
face = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] in ('io', 'gr') else 'io'
d = np.load(os.path.join(ROOT, 'build', 'meters', 'needle_set_%s.npz' % face))
fr, ks, bgH = d['frames'].astype(np.float32), list(d['k']), d['bg']
want = [int(a) for a in sys.argv[2:] if a.isdigit()] or [k for k in (4, 14, 24, 34, 45, 55, 66) if k in ks]


def render(deg):
    subprocess.run([VR, TMP, face, str(deg)], check=True, cwd=ROOT)
    a = np.asarray(Image.open(TMP).convert('RGBA')).astype(np.float32)
    out = np.zeros(fr.shape[1:3] + (4,), np.float32)          # into the crop (170, 50) .. (470, 285)
    out[73 - 50:73 - 50 + a.shape[0], 197 - 170:197 - 170 + a.shape[1]] = a
    return out


if __name__ == "__main__":
    bgW = render(150)                                        # our dial with the needle off the window
    tiles, errs = [], []
    for k in want:
        w = render(DEG[k]); h = fr[ks.index(k)]
        mask = (w[..., 3] > 254) & (bgW[..., 3] > 254)
        dH = h - bgH; dW = w[..., :3] - bgW[..., :3]               # each needle's change of its own dial
        near = mask & ((np.abs(dH).max(2) > 6) | (np.abs(dW).max(2) > 6))
        e = (dW - dH)[near]
        errs.append((k, np.abs(e).mean(), np.sqrt((e ** 2).mean()), near.sum()))
        y0, y1, x0, x1 = 20, 175, 20, 285
        hh = np.clip(bgW[..., :3] + dH, 0, 255)                    # H-Comp's needle on our dial, for an even view
        diff = np.clip(128 + 2 * (dW - dH).mean(2, keepdims=True).repeat(3, 2), 0, 255)
        tiles.append(np.concatenate([hh[y0:y1, x0:x1], w[y0:y1, x0:x1, :3], diff[y0:y1, x0:x1]], 1))
    for k, ma, rms, n in errs: print('k %2d (%+d deg): needle-area mean abs error %.1f, rms %.1f (%d px)' % (k, DEG[k], ma, rms, n))
    img = np.concatenate(tiles[:4], 0)
    Image.fromarray(img.astype(np.uint8)).resize((img.shape[1] * 2, img.shape[0] * 2), Image.NEAREST).save(os.path.join(ROOT, 'build', 'meters', 'cmp_needle_%s.png' % face))
