"""Phase 4: the doubler (Pitch) fitted on the reference's wet output (the output with the colour on less the output
with it clear; white noise, every other colour clear, the send after the fixed filter): per side one voice, the input
delayed by d(t) = centre + amp cos (2 pi f t + phase) (t from the plug-in's start: cv_ref's 1 s pre-roll), times a gain,
through two shelves (the preset's 400 Hz / 4 kHz, their gains fitted; Spreader without); the parameters searched for the
deepest null of the wet output. Starts from the tracked curves (MEASUREMENTS.md). Writes doubler_params.json.

    python -I fit_voxchain_doubler.py [colour ...]
"""
import json
import os
import sys
import numpy as np
from scipy.optimize import minimize
from scipy.signal import lfilter, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cv_ref as C
import fit_voxchain_eq as E
import fit_voxchain_deess as Fd
FS = 48000
OUT = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'doubler_params.json')
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)
START = {1: [[10.065, 2.616, 0.1998, 3.202, 0.2655], [16.717, 1.742, 0.3, 3.216, 0.2650]],
         2: [[6.00, 2.17, 0.2, 2.34, 0.2651], [35.26, 4.26, 0.3035, 6.13, 0.2660]],
         3: [[8.35, 3.07, 0.15, 5.17, 0.2668], [6.33, 3.01, 0.15, 1.33, 0.2890]]}


def warp(x, dms, t0):
    """x read d(t) ms back (a 32-tap windowed sinc); t0: the plug-in time of x's first sample"""
    t = np.arange(len(x)) - dms * FS / 1000.0
    i = np.floor(t).astype(int); fr = t - i
    out = np.zeros(len(x))
    for k in range(-15, 17):
        idx = i + k; ok = (idx >= 0) & (idx < len(x)); arg = fr - k
        wv = np.sinc(arg) * (0.5 + 0.5 * np.cos(np.pi * arg / 16)) * (np.abs(arg) < 16)
        out[ok] += x[idx[ok]] * wv[ok]
    return out


def voice(x, p, shelves, t0=1.0):
    tt = np.arange(len(x)) / FS + t0
    d = p[0] + p[1] * np.cos(2 * np.pi * p[2] * tt + p[3])
    v = p[4] * warp(x, d, t0)
    if shelves is not None:
        b, a = E.biquad('ls', 400.0, 0.7071, shelves[0]); v = lfilter(b, a, v)
        b, a = E.biquad('hs', 4000.0, 0.7071, shelves[1]); v = lfilter(b, a, v)
    return v


def main():
    cols = [int(a) for a in sys.argv[1:]] or [1, 2, 3]
    res = json.load(open(OUT)) if os.path.exists(OUT) else {}
    x = (0.05 * np.random.default_rng(9).standard_normal(FS * 5)).astype(np.float32)
    y0 = C.render(x, FS, {C.NOISE: 0}, C.host(**CLEAR))
    send = sosfilt(Fd.fixed_sos(), x.astype(float))                 # (the send is taken after the fixed filter)
    for c in cols:
        y = C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, pitch_color=c)))
        w = (y - y0).astype(float)
        sh = [-8.6, 4.5] if c < 3 else None
        out = []
        for side in (0, 1):
            seg = slice(FS // 2, None)
            def null(q):
                p = q[:5]; s2 = list(q[5:7]) if sh is not None else None
                v = voice(send, p, s2)
                return 10 * np.log10(np.sum((w[side, seg] - v[seg]) ** 2) / np.sum(w[side, seg] ** 2))
            q0 = np.array(START[c][side] + (sh if sh is not None else []), float)
            best = (null(q0), q0)
            # the phase first (a coarse scan), then everything
            for ph in np.linspace(0, 2 * np.pi, 24, endpoint=False):
                q = best[1].copy(); q[3] = ph
                v = null(q)
                if v < best[0]: best = (v, q)
            r = minimize(null, best[1], method='Nelder-Mead', options=dict(maxiter=1500, xatol=1e-5, fatol=1e-3, adaptive=True))
            q = r.x if r.fun < best[0] else best[1]
            out.append([float(v) for v in q])
            print('colour %d %s: null %.1f dB (start %.1f): centre %.4f amp %.4f f %.5f phase %.4f gain %.4f%s' % (
                c, 'LR'[side], min(r.fun, best[0]), null(q0), q[0], q[1], q[2], q[3], q[4], (' shelves %.2f %.2f' % (q[5], q[6])) if sh else ''), flush=True)
        res[str(c)] = out
        json.dump(res, open(OUT, 'w'), indent=1)


if __name__ == '__main__':
    main()
