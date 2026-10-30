"""Phase 4: Compress, per colour and fader step (-10 .. +10), fitted on the reference with every other colour clear:
a 1 kHz tone ladder (-45 .. +6 dBFS, the static curve), two tone bursts (-30 -> -6 -> -30 and -20 -> 0 -> -20 dBFS: the
timing) and two loud held-out vocals. The model (comp.c) followed by the chain's fixed filter is nulled against the
reference; its parameters are searched (Nelder-Mead, each step starting from its neighbour's) for the deepest mean null.

    python -I fit_voxchain_compress.py render          (the reference's renders, reference/wvoxchain/meas/compress/)
    python -I fit_voxchain_compress.py fit [colour]    (writes reference/wvoxchain/meas/compress_params.json)
    python -I fit_voxchain_compress.py refit [colour]  (the second pass: the main de-esser after the fixed filter in the
                                                        model, and the static correction solved from the tone ladder)
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
         'a2', 'b', 'a3', 'shapeAfter'] + ['corr%+d' % l for l in range(-45, 7, 3)] + ['order', 'feedback', 'limGin', 'detHP']
NP = len(NAMES)
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


def low_signals():
    """the low tones' ladders (100 and 250 Hz, -36 .. 0 dBFS): the compression's frequency dependence and its ripple
    within a cycle (the harmonics), which the 1 kHz ladder does not show"""
    out = []
    n = int(0.4 * FS); t = np.arange(n) / FS
    for f0 in (100, 250):
        out.append(np.concatenate([10 ** (l / 20) * np.sin(2 * np.pi * f0 * t) for l in (-36, -24, -18, -12, -6, 0)]))
    return [np.stack([s, s]).astype(np.float32) for s in out]


def _render_low(job):
    import cv_ref as C
    c, v = job
    path = os.path.join(MEAS, 'c%d_%+03d_low.npz' % (c, v))
    if os.path.exists(path): return path
    X = low_signals()
    Y = [C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, compress_color=c, compress=v))) for x in X]
    np.savez(path, **{'x%d' % i: x for i, x in enumerate(X)}, **{'y%d' % i: y for i, y in enumerate(Y)})
    return path


def render_low():
    jobs = [(c, v) for c in (1, 2, 3) for v in range(-10, 11)]
    with Pool(12) as pool:
        for i, p in enumerate(pool.imap_unordered(_render_low, jobs)): pass
    print('rendered %d positions' % len(jobs))


def noise_signals():
    """a steady noise ladder (150 Hz .. 4 kHz, -36 .. -6 dBFS rms): a crest factor a sine does not have, so the
    detector's reading of peaks against power (on steady loud noise the first fits compressed up to 1 dB too little,
    on sustained vocals too much)"""
    from scipy.signal import butter, sosfilt
    rng = np.random.default_rng(21)
    n = int(0.5 * FS); lv = (-36, -30, -24, -18, -12, -6)
    nz = sosfilt(butter(2, [150, 4000], 'bandpass', fs=FS, output='sos'), rng.standard_normal(n * len(lv)))
    s = np.concatenate([nz[k * n:(k + 1) * n] / np.sqrt(np.mean(nz[k * n:(k + 1) * n] ** 2)) * 10 ** (l / 20) for k, l in enumerate(lv)])
    return [np.stack([s, s]).astype(np.float32)]


def _render_extra(job):
    import cv_ref as C
    c, v, kind = job
    path = os.path.join(MEAS, 'c%d_%+03d_%s.npz' % (c, v, kind))
    if os.path.exists(path): return path
    X = low_signals() if kind == 'low' else noise_signals()
    Y = [C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, compress_color=c, compress=v))) for x in X]
    np.savez(path, **{'x%d' % i: x for i, x in enumerate(X)}, **{'y%d' % i: y for i, y in enumerate(Y)})
    return path


def render_noise():
    jobs = [(c, v, 'noise') for c in (1, 2, 3) for v in range(-10, 11)]
    with Pool(12) as pool:
        for i, p in enumerate(pool.imap_unordered(_render_extra, jobs)): pass
    print('rendered %d positions' % len(jobs))


def load_data(c, v):
    """a step's cases: the ladder, two bursts, two vocals, then (when rendered) the low tones' ladders and the noise
    ladder"""
    d = np.load(os.path.join(MEAS, 'c%d_%+03d.npz' % (c, v)))
    data = [(d['x%d' % i].astype(np.float64), d['y%d' % i].astype(np.float64)) for i in range(5)]
    for kind, count in (('low', 2), ('noise', 1)):
        path = os.path.join(MEAS, 'c%d_%+03d_%s.npz' % (c, v, kind))
        if os.path.exists(path):
            d = np.load(path)
            data += [(d['x%d' % i].astype(np.float64), d['y%d' % i].astype(np.float64)) for i in range(count)]
    return data


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
DEESS = [False]                                                    # the main de-esser after the fixed filter (refit)
_D = None


def deess(z):
    global _D
    import fit_voxchain_deess as Fd
    if _D is None:
        js = json.load(open(Fd.PARAMS)); _D = (Fd, js['h'], np.array(js['p']), js['la'])
    Fd, h, p, la = _D
    return Fd.model(z, Fd.band(z, h), p, la)


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
    z = np.stack([z, z])
    return deess(z) if DEESS[0] else z


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


def pad(p):
    p = list(p)
    return np.array(p + [0.0] * (NP - len(p)), float)


def solve_corr(p, x, y, rounds=4):
    """the static correction from the tone ladder: each step's steady 1 kHz gain error (reference over model, dB) taken
    off the reduction at that step's level (its knot), a few rounds"""
    n = FS // 2; t = np.arange(n) / FS; e1 = np.exp(-2j * np.pi * 1000 * t[n // 2:])
    p = p.copy()
    for _ in range(rounds):
        z = model(x, p)[0]
        for i in range(18):
            s = slice(i * n + n // 2, (i + 1) * n)
            e = 20 * np.log10(abs(np.mean(y[0, s] * e1)) / max(abs(np.mean(z[s] * e1)), 1e-30))
            p[19 + i] = float(np.clip(p[19 + i] - 0.9 * e, -8, 8))
    return p


# the chain's L1 sub-presets per colour (threshold, output; RESEARCH.md): the limiter-first order starts from them
L1 = {1: (-3.0, -6.0), 2: (-7.0, -7.0), 3: (-11.0, -8.0)}
FORCE = [False]


def refit_one(args):
    """one step: from its first fit (or its neighbour's in the limiter-first order), rounds of the ladder's correction
    (kept only when the mean null improves) and Nelder-Mead; the best of all is kept"""
    c, v, p0, order, fb = args
    COLOUR[0] = c; DEESS[0] = True
    data = load_data(c, v)
    p = pad(p0)
    start = nulls(p, data)
    first = [float(np.mean(start)), p.copy()]
    if order == 1 and p[37] < 0.5:
        p[37] = 1; p[39] = -L1[c][0]; p[8] = L1[c][1]
    p[38] = fb
    if len(data) > 5 and p[40] <= 0: p[40] = 20.0                      # (the detector's high-pass, from nearly off)
    free = FREE + ([39] if order == 1 else []) + ([40] if len(data) > 5 else [])
    best = [float(np.mean(nulls(p, data))), p.copy()]
    if first[0] < best[0] and not FORCE[0]: best = first             # (FORCE: keep the asked structure, to compare)
    for rnd in range(3):
        q = solve_corr(best[1], *data[0])
        val = float(np.mean(nulls(q, data)))
        if val < best[0]: best[0] = val; best[1] = q
        base = best[1].copy()
        def obj(q):
            pp = base.copy(); pp[free] = q
            if pp[2] < 1 or pp[3] < 0 or pp[4] < 0.01 or pp[5] < 0.5 or pp[6] < 0.5 or not (0 <= pp[7] <= 1) or pp[9] < 0.5 or pp[13] < 0.05 or pp[14] < 0: return 1e3
            val = float(np.mean(nulls(pp, data)))
            if val < best[0]: best[0] = val; best[1] = pp.copy()
            return val
        minimize(obj, base[free], method='Nelder-Mead', options=dict(maxiter=600 if rnd == 0 else 400, xatol=1e-4, fatol=1e-3, adaptive=True))
    p = best[1]
    return c, v, p.tolist(), nulls(p, data), start


def refit():
    """refit colour:order:feedback ...  (order 0: the compressor first, 1: the limiter first; feedback 1: the compressor's
    detector on its output). The defaults are what a structure test (each order and detector at four steps) found best: Push and Wall the
    compressor first, feed-forward; Spank the limiter first, feedback"""
    orders = dict((int(a.split(':')[0]), (int(a.split(':')[1]), int(a.split(':')[2]))) for a in sys.argv[2:]) or {1: (0, 0), 2: (1, 1), 3: (0, 0)}
    res = json.load(open(PARAMS))
    t0 = time.time()
    # (a step already refitted, its parameters complete, is kept: a stopped run resumes)
    jobs = [(c, v, res['%d %+d' % (c, v)]['p'], o, f) for c, (o, f) in orders.items() for v in range(-10, 11)
            if len(res['%d %+d' % (c, v)]['p']) < NP]
    with Pool(6) as pool:
        for c, v, p, ns, st in pool.imap_unordered(refit_one, jobs):
            res['%d %+d' % (c, v)] = dict(p=p, nulls=ns)
            json.dump(res, open(PARAMS, 'w'), indent=1)
            print('c%d %+3d: %s  (was %s; %.0f s)' % (c, v, ' '.join('%.1f' % n for n in ns), ' '.join('%.1f' % n for n in st), time.time() - t0), flush=True)
    print('done in %.0f s' % (time.time() - t0))


def refit_low():
    """refit_low [colour ...]: every step again with the low tones' ladders in its cases and the detector's high-pass
    free, each in the structure it has (a stopped run resumes: steps with every parameter are kept)"""
    cols = [int(a) for a in sys.argv[2:]] or [1, 2, 3]
    res = json.load(open(PARAMS))
    t0 = time.time()
    jobs = []
    for c in cols:
        for v in range(-10, 11):
            p = res['%d %+d' % (c, v)]['p']
            if len(p) >= NP: continue
            q = pad(p); jobs.append((c, v, p, int(q[37] > 0.5), int(q[38] > 0.5)))
    with Pool(6) as pool:
        for c, v, p, ns, st in pool.imap_unordered(refit_one, jobs):
            res['%d %+d' % (c, v)] = dict(p=p, nulls=ns)
            json.dump(res, open(PARAMS, 'w'), indent=1)
            print('c%d %+3d: %s  (was %s; %.0f s)' % (c, v, ' '.join('%.1f' % n for n in ns), ' '.join('%.1f' % n for n in st), time.time() - t0), flush=True)
    print('done in %.0f s' % (time.time() - t0))


def _forced(args):
    FORCE[0] = True
    return refit_one(args)


def restructure():
    """restructure colour order feedback: every step of the colour refitted in that structure (forced, from its present
    fit) on all its cases; a step keeps the new fit only where its mean null beats the present one's on the same cases"""
    c, order, fb = (int(a) for a in sys.argv[2:5])
    res = json.load(open(PARAMS))
    t0 = time.time()
    with Pool(6) as pool:
        base = dict(((b[0], b[1]), b[2]) for b in pool.map(_baseline, [(c, v, res['%d %+d' % (c, v)]['p']) for v in range(-10, 11)]))
        jobs = [(c, v, res['%d %+d' % (c, v)]['p'], order, fb) for v in range(-10, 11)]
        for c_, v, p, ns, st in pool.imap_unordered(_forced, jobs):
            key = '%d %+d' % (c_, v)
            keep = np.mean(ns) < base[(c_, v)]
            if keep: res[key] = dict(p=p, nulls=ns)
            json.dump(res, open(PARAMS, 'w'), indent=1)
            print('c%d %+3d: order %d fb %d %s (mean %.2f) against %.2f: %s  (%.0f s)' % (c_, v, order, fb, ' '.join('%.1f' % n for n in ns),
                  np.mean(ns), base[(c_, v)], 'kept' if keep else 'not kept', time.time() - t0), flush=True)
    print('done in %.0f s' % (time.time() - t0))


def _baseline(args):
    c, v, p0 = args
    COLOUR[0] = c; DEESS[0] = True
    return c, v, float(np.mean(nulls(pad(p0), load_data(c, v))))


def rescue():
    """rescue colour ...: where a refit in another structure left a step worse than its first fit (with the de-esser),
    refit that step from its first fit in the first structure (the compressor first, feed-forward) and keep the better"""
    cols = [int(a) for a in sys.argv[2:]] or [2]
    res = json.load(open(PARAMS))
    first = json.load(open(PARAMS.replace('.json', '.before_refit.json')))
    t0 = time.time()
    with Pool(6) as pool:
        base = pool.map(_baseline, [(c, v, first['%d %+d' % (c, v)]['p']) for c in cols for v in range(-10, 11)])
        worse = [(c, v, first['%d %+d' % (c, v)]['p'], 0, 0) for c, v, b in base if np.mean(res['%d %+d' % (c, v)]['nulls']) > b]
        print('%d steps worse than their first fit: %s' % (len(worse), ' '.join('c%d %+d' % (w[0], w[1]) for w in worse)), flush=True)
        for c, v, p, ns, st in pool.imap_unordered(refit_one, worse):
            key = '%d %+d' % (c, v)
            if np.mean(ns) < np.mean(res[key]['nulls']): res[key] = dict(p=p, nulls=ns)
            json.dump(res, open(PARAMS, 'w'), indent=1)
            print('c%d %+3d: first structure %s (mean %.2f), kept %.2f  (%.0f s)' % (c, v, ' '.join('%.1f' % n for n in ns), np.mean(ns), np.mean(res[key]['nulls']), time.time() - t0), flush=True)
    print('done in %.0f s' % (time.time() - t0))


if __name__ == '__main__':
    globals()[sys.argv[1]]()
