"""WBass identification: low carriers with the wide level detector pinned by a strong tone (-3 dBFS, an odd multiple
of f0/2 near 200 Hz: no collisions with the carrier's harmonics), Decay 0. The carrier's gain variations then come
almost only from the gain stage driven by l. -> measurements/rbass/pinned.json (cosine-referenced h1..h9)."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; F = 80.0


def comp(y, f, secs=2):
    n = secs * FS; seg = np.asarray(y[-n:], np.float64); t = (np.arange(n) + len(y) - n) / FS
    return 2 * np.mean(seg * np.exp(-2j * np.pi * f * t))


t = np.arange(int(3.5 * FS)) / FS
out = []
for f0 in (10, 12, 15, 18, 22, 27, 33, 40, 48, 56):
    fb = (int(200 / f0) + 0.5) * f0
    for LA in (-40, -30, -20, -12):
        for LB in (None, -3):
            x = 10 ** (LA / 20) * np.sin(2 * np.pi * f0 * t) + (0 if LB is None else 10 ** (LB / 20) * np.sin(2 * np.pi * fb * t))
            x = x.astype(np.float32)
            a = R.render_rbass(x, FS, freq=F, intensity=0, hp=1, extra={8: 0})[0].astype(np.float64)
            b = R.render_rbass(x, FS, freq=F, intensity=-24, hp=1, extra={8: 0})[0].astype(np.float64)
            g = (a - b) / (1 - 10 ** (-24 / 20))
            h = [comp(g, k * f0) for k in range(1, 10, 2)]
            out.append(dict(f0=f0, fb=fb, LA=LA, LB=LB, h=[[v.real, v.imag] for v in h]))
    print(f0, "done", flush=True)
json.dump(out, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "pinned.json"), "w"))
