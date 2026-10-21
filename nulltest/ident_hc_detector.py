"""WComp Phase 4: refine H-Comp's gain path on the clean spike / burst cases (ident_hc_struct.py, output -18 dB so the
output limiter never acts).

Structure (found by ident_hc_struct.py): h = max |x| over 64 look-ahead samples; gt = static gain(h); g1 = one-pole on
the linear gain (attack coefficient falling, release coefficient rising); g = FIR(g1). Here the FIR taps are free
(least squares) and the release time is searched, to see what shape the smoothing really has.

    python -I ident_hc_detector.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.ndimage import maximum_filter1d
import measure_hcomp as M
from ident_hc_struct import cases

FS = 48000
from hcmodel import static_gain_db


def g1_of(a, att_ms, rel_samples, H=64, lead=0, thr=-30.0, rat=50.0):
    pad = np.concatenate([a, np.full(H + lead, a[-1])])
    h = maximum_filter1d(pad, size=H, origin=-(H // 2))[lead:lead + len(a)]
    gt = 10 ** (static_gain_db(h, thr, rat) / 20)
    gt /= gt[0]
    ca = np.exp(-1.0 / (att_ms * 1e-3 * FS)); cr = np.exp(-1.0 / rel_samples) if rel_samples > 0 else 0.0
    o = np.empty_like(gt); s = gt[0]
    for i in range(len(gt)):
        s = gt[i] + (ca if gt[i] < s else cr) * (s - gt[i])
        o[i] = s
    return o


def fit(cs, rel, L=64, H=64, lead=0):
    A, b = [], []
    for att, loud, k, a, g in cs:
        g1 = g1_of(a, att, rel, H, lead)
        idx = np.arange(120, 950)
        A.append(np.stack([1 - g1[idx - j] for j in range(L)], axis=1)); b.append(1 - g[idx])
    A = np.concatenate(A); b = np.concatenate(b)
    f, *_ = np.linalg.lstsq(A, b, rcond=None)
    r = A @ f - b
    return f, np.sqrt(np.mean(r ** 2)), np.abs(r).max()


def main():
    cs = cases()
    for rel in (0, 2, 5, 10, 20):
        for H, lead in ((64, 0), (64, 1), (63, 0)):
            f, rms, mx = fit(cs, rel, H=H, lead=lead)
            print('release %3d samples, hold %d lead %d: rms %.2e max %.2e, FIR sum %.4f' % (rel, H, lead, rms, mx, f.sum()), flush=True)
    f, rms, mx = fit(cs, 0)
    print('taps (release 0, hold 64):', ' '.join('%.4f' % v for v in f))


if __name__ == '__main__':
    main()
