"""WBass: fit of the l-driven gain stage alone on the pinned carrier set (pinned.json, wide detector pinned by a
strong tone), the wide stage held at build/combo.json."""
import sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, wbass_model as M
from scipy.optimize import least_squares
FS = 48000; F = 80.0
B = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
pin = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "pinned.json")))
which = os.environ.get("CASES", "pinned")
cases = [r for r in pin if (r["LB"] is not None) == (which == "pinned") or which == "all"]


def comp(y, f, secs=2.0):
    n = int(round(int(secs * f) * FS / f)); seg = y[-n:]; t = (np.arange(n) + len(y) - n) / FS
    return 2 * np.mean(seg * np.exp(-2j * np.pi * f * t))     # cosine reference, like pinned.json


def model_h(P, r):
    t = np.arange(int(2.6 * FS)) / FS
    x = 10 ** (r["LA"] / 20) * np.sin(2 * np.pi * r["f0"] * t) + (0 if r["LB"] is None else 10 ** (r["LB"] / 20) * np.sin(2 * np.pi * r["fb"] * t))
    y = M.generated7(x, F, P)
    return np.array([comp(y, k * r["f0"]) for k in (1, 3, 5, 7)])


base = dict(json.load(open(os.path.join(B, "combo.json"))), k2=0.0)
keys = os.environ.get("KEYS", "El0,pL,tLa,tLr").split(",")
init = json.loads(os.environ.get("INIT", "{}")); base.update(init)


def mk(v):
    P = dict(base); P.update({k: float(np.exp(x)) for k, x in zip(keys, v)}); return P


def err(v, show=False):
    P = mk(v); e = []
    for r in cases:
        c = np.array([complex(*x) for x in r["h"]])[:4]; m = model_h(P, r)
        for h, w in ((0, 3.0), (1, 1.0), (2, 0.6), (3, 0.3)):
            q = np.log((m[h] + 1e-30) / c[h]); e += [w * q.real, w * np.angle(np.exp(1j * q.imag))]
        if show:
            print("  f0 %2d A %3d B %4s: h1 %+5.2f dB %+4.0f | h3 %+5.1f dB %+4.0f | h5 %+5.1f dB %+4.0f" % (
                r["f0"], r["LA"], r["LB"], *sum([[8.686 * np.log(abs(m[h] / c[h])), np.degrees(np.angle(m[h] / c[h]))] for h in range(3)], [])))
    return np.array(e)


if __name__ == "__main__":
    v0 = np.log([base[k] for k in keys])
    print("start cost %.1f" % (0.5 * np.sum(err(v0) ** 2)), flush=True)
    r = least_squares(err, v0, bounds=(v0 - 6, v0 + 6), diff_step=1e-3, max_nfev=80)
    P = mk(r.x); print("cost %.1f" % r.cost, {k: round(P[k], 6) for k in keys}, flush=True)
    json.dump(P, open(os.path.join(B, "lstage_%s.json" % os.environ.get("TAG", "a")), "w"))
    err(r.x, True)
