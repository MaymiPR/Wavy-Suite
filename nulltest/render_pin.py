"""WBass identification: low carrier tone plus a 'pinning' tone that the wide level detector sees but the l band does
not (Decay 0). Separates the gain stage driven by l itself (it makes the 3rd harmonic of low notes) from the one driven
by the wider band. -> measurements/rbass/pin.json: complex h1, h3, h5 of the carrier."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; F = 80.0


def comp(y, f, secs=2):
    n = secs * FS; seg = np.asarray(y[-n:], np.float64); t = (np.arange(n) + len(y) - n) / FS
    return 2 * np.mean(seg * np.exp(-2j * np.pi * f * t))


t = np.arange(int(3.5 * FS)) / FS
out = []
for f0, fb in ((15, 157.5), (20, 210), (30, 195), (40, 220)):     # fb an odd multiple of f0/2: no collisions
    for LA in (-40, -26, -12):
        for LB in (None, -20, -10, -3):
            x = 10 ** (LA / 20) * np.sin(2 * np.pi * f0 * t) + (0 if LB is None else 10 ** (LB / 20) * np.sin(2 * np.pi * fb * t))
            x = x.astype(np.float32)
            a = R.render_rbass(x, FS, freq=F, intensity=0, hp=1, extra={8: 0})[0].astype(np.float64)
            b = R.render_rbass(x, FS, freq=F, intensity=-24, hp=1, extra={8: 0})[0].astype(np.float64)
            g = (a - b) / (1 - 10 ** (-24 / 20))
            h = [comp(g, k * f0) for k in (1, 3, 5)]
            out.append(dict(f0=f0, fb=fb, LA=LA, LB=LB, h=[[v.real, v.imag] for v in h]))
            print(f0, LA, LB, " ".join("%.1f" % (20 * np.log10(abs(v) + 1e-15)) for v in h), flush=True)
json.dump(out, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "pin.json"), "w"))
