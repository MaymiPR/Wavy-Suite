"""WComp Phase 4: fit a small MLP for H-Comp's effective release time (hcfast2.c hc_set_mlp) on the burst cases.

    python -I ident_hc_mlp.py [nfev]           writes measurements/hcomp/release_mlp.json
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
import ident_hc_release2 as I
from ident_hc_release import load

OUT = os.path.join(os.path.dirname(I.OUT), 'release_mlp.json')


def set_mlp(p):
    I.dll.hc_set_mlp(np.ascontiguousarray(p, np.float64).ctypes.data_as(ct.c_void_p))


def main():
    nfev = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    cs = load()
    x = np.array(json.load(open(I.OUT))['x'])
    rng = np.random.default_rng(1)
    if os.path.exists(OUT): p0 = np.array(json.load(open(OUT))['p'])
    else:
        p0 = rng.normal(0, 0.5, 49)
        p0[48] = -3.0                                  # start fast (w small), as after short bursts
    cases = [c for c in cs if c['att'] == 0.5]
    Rts = [I.target(c) for c in cases]

    def f(p):
        set_mlp(p)
        return I.residuals(x, cases, Rts, dec=16)
    r = f(p0); print('start: rms %.3f worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cases)), flush=True)
    s = optimize.least_squares(f, p0, diff_step=1e-4, max_nfev=nfev)
    r = f(s.x); print('fit: rms %.3f worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'p': list(map(float, s.x))}, open(OUT, 'w'))
    set_mlp(s.x)
    I.report(x, cs)


if __name__ == '__main__':
    main()
