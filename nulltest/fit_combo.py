"""WBass: combined fit of the two-detector gain model: time-domain renders (4 Freq settings + transients, as
fit_multiF2.py) together with the steady harmonic and detector-pinning data (as fit_twodet.py)."""
import sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, wbass_model as M
from scipy.optimize import least_squares
import fit_twodet as H                      # harmonic cases and helpers (its fit does not run on import)
FS = 48000
B = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass")
sets = []
for F in (40, 80, 160, 256):
    d = np.load(os.path.join(D, "fitdata_%d.npz" % F))
    for n in d.files:
        if n.startswith("gen_"): sets.append((F, d["x_" + n[4:]].astype(np.float64), d[n], n[4:], 1))
tr = np.load(os.path.join(D, "transients.npz"))
for F in (80, 160, 256):
    for nm in ("kick", "notes", "riff"):
        sets.append((F, tr["x_%s_%d" % (nm, F)].astype(np.float64), tr["lin_%s_%d" % (nm, F)], "TR_" + nm, 2))
sl = slice(FS // 10, None)
base = json.load(open(os.path.join(B, "multiF2.json")))
td = json.load(open(os.path.join(B, "twodet.json")))
base.update(El0=td["El0"], pL=td["pL"], tLa=td["tLa"], tLr=td["tLr"], sL=0.0)
keys = ["C0", "E0", "p", "cA", "QA", "ta1", "tr1", "ta2", "tr2", "alpha", "k2", "s1", "s2", "El0", "pL", "tLa", "tLr"]
lin = {"s1", "s2", "alpha"}
WH = float(os.environ.get("WH", "0.15"))      # weight of the harmonic block


def mk(v):
    P = dict(base)
    for k, x in zip(keys, v): P[k] = float(x) if k in lin else (-float(np.exp(x)) if k == "k2" else float(np.exp(x)))
    return P


def resid(v):
    P = mk(v); o = []
    for F, x, g, n, w in sets:
        m = M.generated7(x, F, P); o.append(w * (g[sl] - m[sl])[::8] / np.sqrt(np.mean(g[sl] ** 2)))
    Ph = dict(P, k2=0.0)
    e = []
    for f0, LA, fb, LB, c in H.cases:
        m = H.model_h(Ph, f0, LA, fb, LB)
        for h, w in ((0, 3.0), (1, 1.0), (2, 0.5)):
            q = np.log((m[h] + 1e-30) / c[h]); e += [w * q.real, w * np.angle(np.exp(1j * q.imag))]
    o.append(WH * np.array(e))
    return np.concatenate(o)


v0 = np.array([base[k] if k in lin else np.log(abs(base[k])) for k in keys])
lo = np.array([-0.5 if k in ("s1", "s2") else 0.0 if k == "alpha" else -15 for k in keys])
hi = np.array([1.5 if k in ("s1", "s2") else 1.0 if k == "alpha" else 8 for k in keys])
r = least_squares(resid, np.clip(v0, lo + 1e-6, hi - 1e-6), bounds=(lo, hi), diff_step=1e-3, max_nfev=60)
P = mk(r.x); json.dump(P, open(os.path.join(B, "combo.json"), "w"))
print({k: round(P[k], 6) for k in keys})
for F in (40, 80, 160, 256):
    v = [10 * np.log10(np.sum((g[sl] - M.generated7(x, F, P)[sl]) ** 2) / np.sum(g[sl] ** 2)) for FF, x, g, n, w in sets if FF == F]
    print("F %d median %.1f worst %.1f" % (F, np.median(v), max(v)))
