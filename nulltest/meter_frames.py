"""Reads a tools/MeterCapture recording of the WBass / RBass editor: bar fill (px) of the Original Bass, Output L and
Created Harmonics meters per frame, plus the recorded output audio."""
import numpy as np

FS = 48000


def load(path):
    w, h = map(int, open(path + ".txt").read().split())
    raw = np.memmap(path, dtype=np.uint8, mode="r")
    fs = 8 + w * h * 3; n = len(raw) // fs
    fr = raw[:n * fs].reshape(n, fs)
    t = np.array(fr[:, :8]).view(np.float64)[:, 0]
    def fill(x, y0, y1, red=False):
        c = np.array(fr[:, 8:].reshape(n, h, w, 3)[:, y0:y1, x]).astype(int)
        return ((c[..., 0] > 120) & ((c[..., 2] < 90) if not red else (c[..., 1] < 120))).sum(1)
    out = np.fromfile(path + ".pcm", dtype=np.float32).reshape(-1, 2).astype(float)
    return t, dict(o=fill(57, 121, 276), L=fill(309, 130, 278), H=fill(150, 120, 274, True)), out
