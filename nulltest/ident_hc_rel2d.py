"""WComp Phase 4: fit the effective release time as a table over (R, S) (hcfast2.c hc_set_rel2d), on the burst cases.

    python -I ident_hc_rel2d.py [nfev]

Starts from rel_eff = 56.23 ms / (R - S)^1.42 and the slow law in release_law.json; writes release_rel2d.json.
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
import ident_hc_release2 as I
from ident_hc_release import load

R2 = np.array([0.5, 1, 2, 3, 4, 6, 8, 10, 13, 16, 20, 25, 30, 40, 50], float)
S2 = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 11, 15, 20, 30], float)
OUT = os.path.join(os.path.dirname(I.OUT), 'release_rel2d.json')


def set_table(v):
    I.dll.hc_set_rel2d(len(R2), R2.ctypes.data_as(ct.c_void_p), len(S2), S2.ctypes.data_as(ct.c_void_p),
                       np.ascontiguousarray(v, np.float64).ctypes.data_as(ct.c_void_p))


def initial():
    d = np.maximum(R2[:, None] - S2[None, :], 0.05)
    return np.log(56.23 / d ** 1.42).ravel()


def main():
    nfev = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    cs = load()
    x = np.array(json.load(open(I.OUT))['x'])
    v0 = np.array(json.load(open(OUT))['v']) if os.path.exists(OUT) else initial()
    cases = [c for c in cs if c['dur'] < 2.0 and c['att'] == 0.5] + [c for c in cs if c['dur'] == 2.0 and c['att'] == 0.5][::4]
    Rts = [I.target(c) for c in cases]

    def f(v):
        set_table(v)
        return I.residuals(x, cases, Rts, dec=16)
    r = f(v0); print('start: rms %.3f worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cases)), flush=True)
    s = optimize.least_squares(f, v0, diff_step=1e-3, max_nfev=nfev, verbose=0)
    r = f(s.x); print('fit: rms %.3f worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'R': R2.tolist(), 'S': S2.tolist(), 'v': list(map(float, s.x))}, open(OUT, 'w'), indent=1)
    set_table(s.x)
    I.report(x, cs)


if __name__ == '__main__':
    main()
