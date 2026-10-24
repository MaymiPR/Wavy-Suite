"""Measures the reference H-Delay editor on its screenshots (reference/hdelay, git-ignored): each knob's centre and radius
(the connected non-panel blob around the GUIXML rectangle's centre, its outline fitted with a circle), and the printed
ink around it. Numbers only; no pixels are kept.

    python -I tools/measure_hdelay_ui.py [reference/hdelay/st_1_hover.png]
"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

SHOT = sys.argv[1] if len(sys.argv) > 1 else "reference/hdelay/st_1_hover.png"
TOP = 19                                     # the toolbar's height above the GUIXML view
KNOBS = {                                    # GUIXML rectangles (view coordinates)
    "Delay": (130, 27, 152, 154), "Feedback": (442, 27, 152, 154),
    "Depth": (134, 175, 96, 106), "Rate": (239, 175, 96, 106), "HiPass": (360, 175, 96, 106), "LoPass": (495, 175, 96, 106),
    "Mix": (663, 26, 73, 76), "Output": (663, 106, 73, 76), "Analog": (663, 191, 73, 76), "Link": (454, 199, 44, 53),
}


def panel_mask(I):
    r, g, b = I[..., 0], I[..., 1], I[..., 2]
    return (r > 120) & (r > g + 40) & (g > b + 25)          # the orange plate (also its shaded parts)


def fit_circle(xs, ys):
    A = np.c_[2 * xs, 2 * ys, np.ones_like(xs)]
    c, *_ = np.linalg.lstsq(A, xs ** 2 + ys ** 2, rcond=None)
    return c[0], c[1], np.sqrt(c[2] + c[0] ** 2 + c[1] ** 2)


def main():
    I = np.asarray(Image.open(SHOT).convert("RGB")).astype(float)
    pm = panel_mask(I)
    lum = I.mean(axis=2)
    for name, (l, t, w, h) in KNOBS.items():
        cx, cy = l + w / 2, t + h / 2 + TOP
        x0, x1, y0, y1 = int(l) - 4, int(l + w) + 4, int(t + TOP) - 4, int(t + h + TOP) + 4
        sub = ~pm[y0:y1, x0:x1]
        lab, _ = ndimage.label(sub)
        k = lab[int(cy) - y0, int(cx) - x0]
        if k == 0: print(name, "no blob"); continue
        blob = lab == k
        edge = blob & ~ndimage.binary_erosion(blob)
        ys, xs = np.nonzero(edge)
        # keep the outline's upper-left half (the lower right merges with the cast shadow)
        X, Y, R = fit_circle(xs.astype(float), ys.astype(float))
        sel = (xs - X) + (ys - Y) < 0
        X, Y, R = fit_circle(xs[sel].astype(float), ys[sel].astype(float))
        # the dark cap: pixels darker than 70 inside the blob
        dark = blob & (lum[y0:y1, x0:x1] < 70)
        dy, dx = np.nonzero(dark)
        rc = np.sqrt(dark.sum() / np.pi)
        print("%-9s centre %.2f, %.2f (view y %.2f)  outline r %.2f  dark area r %.2f, its centroid %.2f, %.2f" % (
            name, X + x0 + 0.5, Y + y0 + 0.5, Y + y0 + 0.5 - TOP, R, rc, dx.mean() + x0 + 0.5, dy.mean() + y0 + 0.5))


if __name__ == "__main__":
    main()
