"""WBass identification step 3: harmonic responses vs fundamental frequency at fixed levels, to place the filters before or
after the nonlinearity. Original Bass out, Intensity 0. For each Freq setting, level and f0, stores the complex output
component of harmonics 1..8 (phase referred to the input sine, i.e. divided by exp(j h phi_x)).
Writes measurements/rbass/sweep.json."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "sweep.json")
NH = 8


def comps(y, f0, nh, secs=1.0):
    per = FS / f0; n = int(round(max(1, int(secs * f0)) * per))
    seg = np.asarray(y[-n:], np.float64); t = (np.arange(n) + (len(y) - n)) / FS
    w = np.hanning(n); w /= w.sum() / 2
    # reference: input sine sin(2 pi f0 t) = Im(exp(j 2 pi f0 t)); component c_h such that y ~ sum Re(c_h * exp(j 2 pi h f0 t)) * ... use
    # complex amplitude relative to exp(j(2 pi h f0 t - pi/2)) so a pure sin(h w t) gives 1.
    return np.array([np.sum(seg * w * np.exp(-1j * (2 * np.pi * f0 * h * t - np.pi / 2))) for h in range(1, nh + 1)])


f0s = np.unique(np.round(np.geomspace(12, 110, 26), 1))
res = {"f0": f0s.tolist(), "runs": []}
for fset in (160.0, 64.0):
    for L in (-40.0, -20.0, -6.0):
        rows = []
        for f0 in f0s:
            if f0 > fset * 0.7:
                continue
            x = R.tone(FS, f0, max(1.5, 30.0 / f0), L)
            y = R.render_rbass(x, FS, freq=fset, intensity=0, hp=1)[0]
            c = comps(y, f0, NH)
            rows.append({"f0": float(f0), "re": c.real.tolist(), "im": c.imag.tolist()})
        res["runs"].append({"freq": fset, "level": L, "rows": rows})
        print("freq", fset, "level", L, "done", flush=True)
        json.dump(res, open(OUT, "w"))
