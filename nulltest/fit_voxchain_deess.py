"""Phase 4: the main de-esser (always in the chain), fitted on the reference with every colour clear: loud held-out
vocals at several levels and Sensitivity settings, and tone steps (6 / 10 kHz, with and without low tones). The model
(deess.c: split band, stereo-linked, adaptive threshold) followed by the chain's fixed filter is nulled against the
reference; its parameters are searched for the deepest null (the average of the cases' nulls in dB).

    python -I fit_voxchain_deess.py [render] [fit]
"""
import ctypes
import json
import os
import re
import sys
import numpy as np
from scipy.signal import sosfilt
from scipy.optimize import minimize

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
DATA = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'deess_cases.npz')
PARAMS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'deess_params.json')
FS = 48000
LIB = ctypes.CDLL(os.path.join(HERE, 'deess.dll'))
D = ctypes.POINTER(ctypes.c_double)
LIB.deess.argtypes = [D, D, D, D, D, ctypes.c_int, ctypes.c_double, D]
NAMES = ['fc', 'Q', 'thr', 'slope', 'knee', 'range', 'hAtt', 'hRel', 'adOff', 'lAtt', 'lRel', 'adW', 'rms', 'hold', 'dfc', 'dQ']
START = [5000, 0.42, -20, 1.0, 0.0, 48, 0.5, 1.0, -12, 24.9, 248.7, 1.0, 0, 0.0, 0.0, 0.7]


def fixed_sos():
    """the fixed filter's sections at 48 kHz from WVoxChainFixed.h (b0 b1 b2 a1 a2 per section)"""
    txt = open(os.path.join(HERE, '..', 'plugins', 'WVoxChain', 'WVoxChainFixed.h')).read()
    body = txt[txt.index('{ 48000.0,') + len('{ 48000.0,'):]
    body = body[:body.index('} } }')]
    v = [float(t) for t in re.findall(r"[-+]?\d[\d.eE+-]*", body)]
    rows = np.array(v).reshape(-1, 5)
    return np.array([[r[0], r[1], r[2], 1.0, r[3], r[4]] for r in rows])


SOS = None


def model(x, p):
    global SOS
    if SOS is None: SOS = fixed_sos()
    xl = np.ascontiguousarray(x[0], np.float64); xr = np.ascontiguousarray(x[1], np.float64)
    yl = np.zeros_like(xl); yr = np.zeros_like(xr)
    pp = np.ascontiguousarray(p, np.float64)
    LIB.deess(xl.ctypes.data_as(D), xr.ctypes.data_as(D), yl.ctypes.data_as(D), yr.ctypes.data_as(D), None, len(xl), float(FS), pp.ctypes.data_as(D))
    return np.stack([sosfilt(SOS, yl), sosfilt(SOS, yr)])


def cases():
    import vocal_corpus as V
    t = np.arange(FS * 3) / FS
    out = []
    for k, pk in ((24, 0), (25, -4), (26, -8), (27, -2), (28, 0)):
        x = V.part(k)[0][:FS * 8]; x = x / np.abs(x).max() * 10 ** (pk / 20)
        out.append(('vocal %d peak %+d' % (k, pk), np.stack([x, x]), 0))
    x = V.part(29)[0][:FS * 8]; x = x / np.abs(x).max() * 10 ** (-10 / 20)
    out.append(('vocal 29 peak -10, sens +8', np.stack([x, x]) * 10 ** (8 / 20), 0))
    x2 = V.part(31)[0][:FS * 8]; x2 = x2 / np.abs(x2).max()
    out.append(('vocal 31 L, 30 R', np.stack([x2, V.part(30)[0][:FS * 8] / np.abs(V.part(30)[0][:FS * 8]).max() * 0.5]), 0))
    for f0 in (6000, 10000):
        env = np.concatenate([np.full(FS, 10 ** (-30 / 20)), np.full(FS, 10 ** (-3 / 20)), np.full(FS, 10 ** (-30 / 20))])
        s = env * np.sin(2 * np.pi * f0 * t)
        out.append(('%d Hz step' % f0, np.stack([s, s]), 0))
    s = 10 ** (-6 / 20) * np.sin(2 * np.pi * 6000 * t) + 10 ** (-10 / 20) * np.sin(2 * np.pi * 300 * t)
    out.append(('6 kHz with 300 Hz', np.stack([s, s]), 0))
    return out


def render():
    import cv_ref as C
    clear = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)
    X, Y, names = [], [], []
    for name, x, _ in cases():
        y = C.render(x.astype(np.float32), FS, {C.NOISE: 0}, C.host(**clear))
        X.append(x); Y.append(y); names.append(name)
        print('rendered', name, flush=True)
    np.savez(DATA, names=np.array(names), **{'x%d' % i: v for i, v in enumerate(X)}, **{'y%d' % i: v for i, v in enumerate(Y)})


def nulls(p, data):
    out = []
    for x, y in data:
        z = model(x, p)
        out.append(10 * np.log10(np.sum((y - z) ** 2) / np.sum(y ** 2) + 1e-30))
    return out


def fit():
    d = np.load(DATA)
    names = list(d['names'])
    data = [(d['x%d' % i].astype(np.float64), d['y%d' % i].astype(np.float64)) for i in range(len(names))]
    p0 = np.array(json.load(open(PARAMS))['p'] if os.path.exists(PARAMS) else START, float)
    print('start:', ' '.join('%s %.1f' % (n, v) for n, v in zip(names, nulls(p0, data))))
    free = [0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 13]
    best = [1e9, p0.copy()]
    def obj(q):
        p = p0.copy(); p[free] = q
        if p[1] <= 0.05 or p[6] < 0 or p[7] < 0 or p[9] < 0 or p[10] < 0 or p[13] < 0 or p[3] < 0 or p[4] < 0: return 1e3
        v = float(np.mean(nulls(p, data)))
        if v < best[0]: best[0] = v; best[1] = p.copy()
        return v
    for rms in (0, 1):
        p0[12] = rms
        r = minimize(obj, p0[free], method='Nelder-Mead', options=dict(maxiter=1500, xatol=1e-3, fatol=1e-3, adaptive=True))
        print('detector %s: mean null %.2f dB' % ('rms' if rms else 'peak', best[0]), flush=True)
        p0 = best[1].copy()
    p = best[1]
    print('best:', ' '.join('%s=%.4g' % (n, v) for n, v in zip(NAMES, p)))
    print('nulls:', ' '.join('%s %.1f' % (n, v) for n, v in zip(names, nulls(p, data))))
    json.dump({'p': [float(v) for v in p], 'names': NAMES}, open(PARAMS, 'w'), indent=1)


if __name__ == '__main__':
    for a in sys.argv[1:] or ['render', 'fit']: globals()[a]()
