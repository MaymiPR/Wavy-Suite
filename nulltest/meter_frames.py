"""Reads a tools/MeterCapture recording of a Renaissance-style editor (RBass / WBass, RVox / WVox): bar fill (px) of
each meter per frame, plus the recorded output audio."""
import numpy as np

FS = 48000

orange = lambda c: (c[..., 0] > 120) & (c[..., 2] < 90)
red = lambda c: (c[..., 0] > 120) & (c[..., 1] < 120)
grred = lambda c: (c[..., 0] > 90) & (c[..., 1] < 70) & (c[..., 2] < 70)
# meter: (column x, first row, end row, lit-pixel test)
LAYOUTS = {
    "bass": dict(o=(57, 121, 276, orange), L=(309, 130, 278, orange), H=(150, 120, 274, red)),
    # Energy stops above the Gate knob; gain reduction is red growing down from the top of the Comp window
    "vox": dict(E=(57, 121, 267, orange), L=(309, 130, 278, orange), G=(160, 121, 276, grred)),
}


def load(path, layout="bass"):
    w, h = map(int, open(path + ".txt").read().split())
    raw = np.memmap(path, dtype=np.uint8, mode="r")
    fs = 8 + w * h * 3; n = len(raw) // fs
    fr = raw[:n * fs].reshape(n, fs)
    t = np.array(fr[:, :8]).view(np.float64)[:, 0]
    img = fr[:, 8:].reshape(n, h, w, 3)
    bars = {k: test(np.array(img[:, y0:y1, x]).astype(int)).sum(1) for k, (x, y0, y1, test) in LAYOUTS[layout].items()}
    out = np.fromfile(path + ".pcm", dtype=np.float32).reshape(-1, 2).astype(float)
    return t, bars, out
