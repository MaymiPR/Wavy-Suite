"""Phase 4: Compress, per colour and fader step (-10 .. +10), fitted on the reference with every other colour clear:
a 1 kHz tone ladder (-45 .. +6 dBFS, the static curve), two tone bursts (-30 -> -6 -> -30 and -20 -> 0 -> -20 dBFS: the
timing) and two loud held-out vocals. The model (comp.c) followed by the chain's fixed filter is nulled against the
reference; its parameters are searched (Nelder-Mead, each step starting from its neighbour's) for the deepest mean null.

    python -I fit_voxchain_compress.py render          (the reference's renders, reference/wvoxchain/meas/compress/)
    python -I fit_voxchain_compress.py fit [colour]    (writes reference/wvoxchain/meas/compress_params.json)
"""
import ctypes
import json
import os
import sys
import time
import numpy as np
from multiprocessing import Pool
from scipy.signal import sosfilt
from scipy.optimize import minimize

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
MEAS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'compress')
PARAMS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'compress_params.json')
FS = 48000
NAMES = ['gin', 'thr', 'ratio', 'knee', 'att', 'rel', 'relSlow', 'slowShare', 'ceil', 'limRel', 'look', 'gout', 'power', 'win', 'limKnee',
         'a2', 'b', 'a3', 'shapeAfter']
SH = [-0.01, 0.01, 0.0, 1]
PUSH0 = [-0.001498, -9.404, 9.606, 5.733, 18.03, 291.6, 0.7521, 0.002176, 0.5733, 7.723, 64, -0.0007511, 1, 0.129, 20.55, 0.01282, 0.006674, 0.002828, 1]
START = {1: PUSH0, 2: list(PUSH0), 3: list(PUSH0)}             # (Push at 0, fitted with its colour's linear filter after)
# the clear chain's own shaper (before the fixed filter; regressed on tones and a vocal): x - 0.00145 x^2 + 0.00146 x^3
PRE = (-0.00144547, 0.00145842)
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)


def signals():
    import vocal_corpus as V
    out = []
    levels = list(range(-45, 7, 3)); n = int(0.5 * FS); t = np.arange(n) / FS
    out.append(np.concatenate([10 ** (l / 20) * np.sin(2 * np.pi * 1000 * t) for l in levels]))
    for lo, hi, on in ((-30, -6, 1.0), (-20, 0, 0.3)):
        env = np.concatenate([np.full(FS // 2, 10 ** (lo / 20)), np.full(int(on * FS), 10 ** (hi / 20)), np.full(FS, 10 ** (lo / 20))])
        out.append(env * np.sin(2 * np.pi * 1000 * np.arange(len(env)) / FS))
    for k, pk in ((24, 0), (26, -6)):
        x = V.part(k)[0][:FS * 6]; out.append(x / np.abs(x).max() * 10 ** (pk / 20))
    return [np.stack([s, s]).astype(np.float32) for s in out]


def _render(job):
    import cv_ref as C
    c, v = job
    path = os.path.join(MEAS, 'c%d_%+03d.npz' % (c, v))
    if os.path.exists(path): return path
    X = signals()
    Y = [C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, compress_color=c, compress=v))) for x in X]
    np.savez(path, **{'x%d' % i: x for i, x in enumerate(X)}, **{'y%d' % i: y for i, y in enumerate(Y)})
    return path


def render():
    os.makedirs(MEAS, exist_ok=True)
    jobs = [(c, v) for c in (1, 2, 3) for v in range(-10, 11)]
    with Pool(12) as pool:
        for i, p in enumerate(pool.imap_unordered(_render, jobs)): pass
    print('rendered %d positions' % len(jobs))


_lib = None
_sos = None


LIN = None
LINPOS = [1]                                                       # the colour's linear filter: 0 before the dynamics, 1 after
COLOUR = [1]


def model(x, p):
    global _lib, _sos, LIN
    from scipy.signal import lfilter
    if LIN is None:
        LIN = json.load(open(os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'compress_linear.json')))
    if _lib is None:
        _lib = ctypes.CDLL(os.path.join(HERE, 'comp.dll'))
        D = ctypes.POINTER(ctypes.c_double)
        _lib.comp.argtypes = [D, D, D, D, D, ctypes.c_int, ctypes.c_double, D]
        import fit_voxchain_deess as Fd
        _sos = Fd.fixed_sos()
    D = ctypes.POINTER(ctypes.c_double)
    x0 = x[0] + PRE[0] * x[0] ** 2 + PRE[1] * x[0] ** 3                 # (the inputs are the same on both sides: one side, linked)
    lin = LIN['48000 %d' % COLOUR[0]]
    if LINPOS[0] == 0: x0 = lfilter(lin['b'], lin['a'], x0)
    xl = np.ascontiguousarray(x0, np.float64); xr = xl
    yl = np.zeros_like(xl); yr = np.zeros_like(xr); pp = np.ascontiguousarray(p, np.float64)
    _lib.comp(xl.ctypes.data_as(D), xr.ctypes.data_as(D), yl.ctypes.data_as(D), yr.ctypes.data_as(D), None, len(xl), float(FS), pp.ctypes.data_as(D))
    if LINPOS[0] == 1: yl = lfilter(lin['b'], lin['a'], yl)
    z = sosfilt(_sos, yl)
    return np.stack([z, z])


def nulls(p, data):
    out = []
    for x, y in data:
        z = model(x, p)
        out.append(10 * np.log10(np.sum((y - z) ** 2) / np.sum(y ** 2) + 1e-30))
    return out


FREE = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 14, 15, 16, 17]


def fit_one(args):
    c, v, p0 = args
    COLOUR[0] = c
    d = np.load(os.path.join(MEAS, 'c%d_%+03d.npz' % (c, v)))
    data = [(d['x%d' % i].astype(np.float64), d['y%d' % i].astype(np.float64)) for i in range(5)]
    p0 = np.array(p0, float); best = [1e9, p0.copy()]
    def obj(q):
        p = p0.copy(); p[FREE] = q
        if p[2] < 1 or p[3] < 0 or p[4] < 0.01 or p[5] < 0.5 or p[6] < 0.5 or not (0 <= p[7] <= 1) or p[9] < 0.5 or p[13] < 0.05 or p[14] < 0: return 1e3
        val = float(np.mean(nulls(p, data)))
        if val < best[0]: best[0] = val; best[1] = p.copy()
        return val
    for shape_after in (0, 1):
        p0 = best[1].copy(); p0[18] = shape_after
        start = best[1].copy(); start[18] = shape_after
        best_before = best[0]
        best[0] = float(np.mean(nulls(start, data))); best[1] = start
        minimize(obj, start[FREE], method='Nelder-Mead', options=dict(maxiter=1500, xatol=1e-4, fatol=1e-3, adaptive=True))
        if shape_after == 0: keep = (best[0], best[1].copy())
    if keep[0] < best[0]: best[0], best[1] = keep
    return c, v, best[1].tolist(), nulls(best[1], data)


def chain(args):
    """one colour, from fader 0 outwards in one direction (each step starting from its neighbour's fit)"""
    c, sign, p = args
    out = []
    for step in range(1, 11):
        r = fit_one((c, sign * step, p))
        p = r[2]; out.append(r)
        print('c%d %+3d: %s' % (c, r[1], ' '.join('%.1f' % n for n in r[3])), flush=True)
    return out


def fit():
    cols = [int(a) for a in sys.argv[2:]] or [1, 2, 3]
    res = json.load(open(PARAMS)) if os.path.exists(PARAMS) else {}
    t0 = time.time()
    # fader 0 of each colour first, then six chains (each colour up and down) in parallel
    with Pool(6) as pool:
        zero = pool.map(fit_one, [(c, 0, res.get('%d +0' % c, {}).get('p', START[c])) for c in cols])
        for r in zero:
            res['%d %+d' % (r[0], r[1])] = dict(p=r[2], nulls=r[3])
            print('c%d +0: %s  (%.0f s)' % (r[0], ' '.join('%.1f' % n for n in r[3]), time.time() - t0), flush=True)
        json.dump(res, open(PARAMS, 'w'), indent=1)
        jobs = [(c, s, res['%d +0' % c]['p']) for c in cols for s in (1, -1)]
        for out in pool.imap_unordered(chain, jobs):
            for r in out: res['%d %+d' % (r[0], r[1])] = dict(p=r[2], nulls=r[3])
            json.dump(res, open(PARAMS, 'w'), indent=1)
    print('done in %.0f s' % (time.time() - t0))


if __name__ == '__main__':
    globals()[sys.argv[1]]()
