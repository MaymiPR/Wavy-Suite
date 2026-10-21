"""WComp Phase 4: fit rel_eff = min(release, release * w0(S), K / (R - S)^p) with the slow state charging from the
output (hcfast2.c), on the burst cases. Writes measurements/hcomp/release_w0.json.

    python -I ident_hc_w0.py [nfev]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
import ident_hc_release2 as I
from ident_hc_release import load

SX = np.array([0, 1, 2, 3, 4, 6, 8, 12, 20, 30], float)
OUT = os.path.join(os.path.dirname(I.OUT), 'release_w0.json')


def apply(z, x):
    w0 = z[:len(SX)]; K, p, crest = np.exp(z[len(SX)]), z[len(SX) + 1], z[len(SX) + 2]
    I.dll.hc_set_w0(len(SX), SX.ctypes.data_as(ct.c_void_p), np.ascontiguousarray(w0).ctypes.data_as(ct.c_void_p), ct.c_double(K), ct.c_double(p))
    y = x.copy(); y[-1] = crest
    return y


def main():
    nfev = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    cs = load()
    x = np.array(json.load(open(I.OUT))['x'])
    I.dll.hc_set_charge_from_r(1)
    z0 = np.array(json.load(open(OUT))['z']) if os.path.exists(OUT) else np.concatenate([np.log(np.interp(SX, [0, 3, 6, 12, 30], [0.004, 0.02, 0.2, 1.0, 1.0])), [np.log(106.0), 1.1, 149.62]])
    cases = [c for c in cs if c['att'] == 0.5]
    Rts = [I.target(c) for c in cases]

    def f(z): return I.residuals(apply(z, x), cases, Rts, dec=16)
    r = f(z0); print('start: rms %.3f worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    s = optimize.least_squares(f, z0, diff_step=1e-3, max_nfev=nfev)
    r = f(s.x); print('fit: rms %.3f worst %.2f; K %.2f p %.3f crest %.1f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), np.exp(s.x[len(SX)]), s.x[len(SX) + 1], s.x[-1]), flush=True)
    print('w0:', ' '.join('%g:%.4f' % (a, np.exp(b)) for a, b in zip(SX, s.x[:len(SX)])))
    json.dump({'S': SX.tolist(), 'z': list(map(float, s.x))}, open(OUT, 'w'))
    I.report(apply(s.x, x), cs)


if __name__ == '__main__':
    main()
