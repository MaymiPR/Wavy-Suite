"""WBass: fit of the two-detector gain model (wbass_model.generated7 with the l-driven stage) on steady harmonics:
the detector-pinning set (pin.json), the Decay-0 tone set across frequency (raw_decay0.json) and level sweeps."""
import sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, wbass_model as M
from scipy.optimize import least_squares
FS = 48000; F = 80.0
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass")
pin = json.load(open(os.path.join(D, "pin.json")))
raw = json.load(open(os.path.join(D, "raw_decay0.json")))
lev = json.load(open(os.path.join(D, "levels_decay0.json")))
cases = []
for r in pin:
    # pin.json uses cosine-referenced complex amplitudes (2 mean(y e^{-jwt})); convert to the sine reference used here
    cases.append((r["f0"], r["LA"], r["fb"], r["LB"], 1j * np.array([complex(*v) for v in r["h"]])))
for L, f0, c in raw:
    if f0 <= 80: c = [complex(*v) for v in c]; cases.append((f0, L, None, None, np.array([c[0], c[2], c[4]])))
for f0s, rows in lev.items():
    for L, c in rows:
        if L in (-48, -30, -18, -6, 0): c = [complex(*v) for v in c]; cases.append((float(f0s), L, None, None, np.array([c[0], c[2], c[4]])))


def comp(y, f, secs=0.5):
    n = int(round(int(secs * f) * FS / f)); seg = y[-n:]; t = (np.arange(n) + len(y) - n) / FS
    w = np.hanning(n); w /= w.sum() / 2
    return np.sum(seg * w * np.exp(-1j * (2 * np.pi * f * t - np.pi / 2)))


def model_h(P, f0, LA, fb, LB):
    # with a pinning tone the analysis needs whole cycles of both tones (2 s), as in render_pin.py
    t = np.arange(int((2.6 if LB is not None else 0.9) * FS)) / FS
    x = 10 ** (LA / 20) * np.sin(2 * np.pi * f0 * t) + (0 if LB is None else 10 ** (LB / 20) * np.sin(2 * np.pi * fb * t))
    y = M.generated7(x, F, P)
    return np.array([comp(y, k * f0, 2.0 if LB is not None else 0.5) for k in (1, 3, 5)])


base = dict(json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "multiF2.json"))), k2=0.0)
base.update(El0=0.01, pL=0.15, tLa=0.002, tLr=0.02, tL2=0.0, sL=0.0)
keys = sys.argv[1].split(",") if len(sys.argv) > 1 else ["C0", "E0", "p", "cA", "QA", "ta1", "tr1", "ta2", "tr2", "El0", "pL", "tLa", "tLr"]


def mk(v):
    P = dict(base); P.update({k: float(np.exp(x)) for k, x in zip(keys, v)}); return P


def err(v, show=False):
    P = mk(v); e = []
    for f0, LA, fb, LB, c in cases:
        m = model_h(P, f0, LA, fb, LB)
        for h, w in ((0, 3.0), (1, 1.0), (2, 0.5)):
            q = np.log((m[h] + 1e-30) / c[h]); e += [w * 8.686 * q.real / 3, w * np.degrees(np.angle(np.exp(1j * q.imag))) / 20]
        if show:
            print("  f0 %2g A %3g B %4s: h1 %+5.2f dB %+4.1f | h3 %+5.1f dB %+4.0f | h5 %+5.1f dB %+4.0f" % (
                f0, LA, LB, *sum([[8.686 * np.log(abs(m[h] / c[h])), np.degrees(np.angle(m[h] / c[h]))] for h in range(3)], [])))
    return np.array(e)


if __name__ == "__main__":
    v0 = np.log([base[k] for k in keys])
    r = least_squares(err, v0, bounds=(v0 - 4, v0 + 4), diff_step=1e-3, max_nfev=120)
    P = mk(r.x)
    print("cost %.1f" % r.cost, {k: round(P[k], 6) for k in keys}, flush=True)
    json.dump(P, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "twodet.json"), "w"))
    err(r.x, True)
