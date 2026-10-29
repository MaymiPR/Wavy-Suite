"""Phase 4: the main de-esser (always in the chain), fitted on the reference with every colour clear. The structure is
measured by probe_deess.py: a zero-phase high band H (its 49 symmetric taps at 48 kHz fitted here to the probe's
response), the output x - (1 - g) H x, g from deess.c's detectors and smoothing (see there). The chain before it (the
clear chain's shaper and the fixed filter) is applied first. The cases: static tones and steps, a high tone under low
tones (the adaptive threshold), bursts of several lengths and levels (the timing), a stereo case, and loud training
vocals; the parameters (and the look-ahead, in samples) are searched for the deepest mean null. Held-out vocals check
it.

    python -I fit_voxchain_deess.py [taps] [render] [fit] [check]
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
MEAS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas')
DATA = os.path.join(MEAS, 'deess_cases.npz')
PARAMS = os.path.join(MEAS, 'deess_params.json')
FS = 48000
NT = 24                                                                # the high band's taps: -NT .. NT
LIB = ctypes.CDLL(os.path.join(HERE, 'deess.dll'))
D = ctypes.POINTER(ctypes.c_double)
LIB.deess.argtypes = [D, D, D, ctypes.c_int, ctypes.c_double, D]
NAMES = ['T', 'k', 'fh', 'Qh', 'fl', 'Ql', 'hAtt', 'hRel', 'lAtt', 'lRel', 'slope', 'range', 'knee',
         'gAtt', 'hold', 'gRel', 'linRel', 'sAtt', 'sRel', 'share', 'power']
START = [-12.8, 0.56, 5000, 1.0, 1400, 0.6, 0.5, 1.0, 24.9, 248.7, 1.0, 48, 1.0,
         0.3, 10.0, 26.0, 1, 50.0, 100.0, 0.0, 0]
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)


def fixed_sos():
    """the fixed filter's sections at 48 kHz from WVoxChainFixed.h (b0 b1 b2 a1 a2 per section)"""
    txt = open(os.path.join(HERE, '..', 'plugins', 'WVoxChain', 'WVoxChainFixed.h')).read()
    body = txt[txt.index('{ 48000.0,') + len('{ 48000.0,'):]
    body = body[:body.index('} } }')]
    v = [float(t) for t in re.findall(r"[-+]?\d[\d.eE+-]*", body)]
    rows = np.array(v).reshape(-1, 5)
    return np.array([[r[0], r[1], r[2], 1.0, r[3], r[4]] for r in rows])


def taps():
    """the high band's symmetric taps, least squares on the probe's response (four drives averaged)"""
    d = np.load(os.path.join(MEAS, 'deess_probe.npz'))
    f = d['f']
    Hs = []
    for k in ('r_12000_0', 'r_12000_6', 'r_16000_0', 'r_16000_6'):
        r = d[k]; g = np.mean(r[(f > 18000) & (f < 20000)]); Hs.append(((1 - r) / (1 - g)).real)
    H = np.mean(Hs, 0)
    H = H.copy(); H[f >= 20000] = 1.0                                   # (the band is whole above 20 kHz)
    m = (f > 20) & (np.abs(f - 12000) > 150) & (np.abs(f - 16000) > 150)
    A = np.stack([np.ones(m.sum())] + [2 * np.cos(2 * np.pi * f[m] * k / FS) for k in range(1, NT + 1)], 1)
    c, *_ = np.linalg.lstsq(A, H[m], rcond=None)
    e = A @ c - H[m]
    print('taps: residual %.1f dB of the band; at 20 Hz %.4f, 1 kHz %.4f, 5 kHz %.4f, 20 kHz %.4f' % (
        10 * np.log10(np.sum(e ** 2) / np.sum(H[m] ** 2)),
        *[c[0] + 2 * sum(c[k] * np.cos(2 * np.pi * q * k / FS) for k in range(1, NT + 1)) for q in (20, 1000, 5000, 20000)]))
    res = json.load(open(PARAMS)) if os.path.exists(PARAMS) else {}
    res['h'] = [float(v) for v in c]
    json.dump(res, open(PARAMS, 'w'), indent=1)


SOS = None


def pre(x):
    """the chain before the de-esser: the clear chain's shaper, then the fixed filter"""
    global SOS
    if SOS is None: SOS = fixed_sos()
    x = np.asarray(x, np.float64)
    x = x - 0.00144547 * x * x + 0.00145842 * x * x * x
    return sosfilt(SOS, x, axis=-1)


def band(u, h):
    """the zero-phase high band"""
    hh = np.concatenate([np.asarray(h)[:0:-1], h])
    return np.stack([np.convolve(c, hh, 'same') for c in u])


def gain(u, p):
    ul = np.ascontiguousarray(u[0]); ur = np.ascontiguousarray(u[1])
    g = np.zeros_like(ul)
    pp = np.ascontiguousarray(p, np.float64)
    LIB.deess(ul.ctypes.data_as(D), ur.ctypes.data_as(D), g.ctypes.data_as(D), len(ul), float(FS), pp.ctypes.data_as(D))
    return g


def model(u, hu, p, la):
    """u: the chain before the de-esser, hu its high band; la: the look-ahead (the gain computed la samples ahead)"""
    g = gain(u, p)
    if la > 0: g = np.concatenate([g[la:], np.full(la, g[-1])])
    return u - (1 - g) * hu


def tone(f0, levels, secs):
    out = []
    for pk in levels:
        n = int(secs * FS); t = np.arange(n) / FS
        out.append(10 ** (pk / 20) * np.sin(2 * np.pi * f0 * t))
    return np.concatenate(out)


def cases(held=False):
    import vocal_corpus as V
    out = []
    if not held:
        for f0 in (3000, 4000, 5000, 6000, 8000, 10000, 16000):
            s = tone(f0, (-30, -12, -6, 0, 6, -30, -9, -3, 3), 0.25)
            out.append(('%d Hz steps' % f0, np.stack([s, s])))
        for fl in (100, 1000, 2000, 3000):
            n = int(0.3 * FS); t = np.arange(n) / FS
            s = np.concatenate([10 ** (-3 / 20) * np.sin(2 * np.pi * 16000 * t) + 10 ** (pk / 20) * np.sin(2 * np.pi * fl * t)
                                for pk in (-40, -24, -12, -6, 0, -18, -3, 3)])
            out.append(('16 kHz -3 with %d Hz' % fl, np.stack([s, s])))
        for f0, pk in ((10000, 0), (10000, -6), (10000, 6), (6000, 0), (4000, 3)):
            seg = []
            for ms in (2, 5, 10, 20, 50, 100, 300, 1000):
                n1 = int(ms * FS / 1000); n2 = int(0.25 * FS)
                env = np.concatenate([np.full(n1, 10 ** (pk / 20)), np.full(n2, 10 ** (-30 / 20))])
                seg.append(env * np.sin(2 * np.pi * f0 * np.arange(n1 + n2) / FS))
            s = np.concatenate(seg)
            out.append(('%d Hz bursts %+d' % (f0, pk), np.stack([s, s])))
        s = tone(10000, (-30, -6, 0, 6, -30), 0.3)
        out.append(('10 kHz L, quiet R', np.stack([s, 0.1 * s[::-1]])))
        rng = np.random.default_rng(7)
        nz = rng.standard_normal(FS * 2) * np.repeat([0.02, 0.1, 0.3, 0.6], FS // 2)
        out.append(('noise steps', np.stack([nz, rng.standard_normal(FS * 2) * np.repeat([0.02, 0.1, 0.3, 0.6], FS // 2)])))
        parts = ((1, 0), (3, 3), (5, -3), (8, 6), (11, 0), (14, 3))
    else:
        parts = ((24, 0), (25, 3), (26, -3), (27, 6), (28, 0), (29, 3), (30, 0), (31, 6))
    for k, pk in parts:
        x = V.part(k)[0][:FS * 8].astype(np.float64); x = x / np.abs(x).max() * 10 ** (pk / 20)
        out.append(('vocal %d peak %+d' % (k, pk), np.stack([x, x])))
    return out


def render(held=False):
    import cv_ref as C
    path = DATA.replace('.npz', '_held.npz') if held else DATA
    X, Y, names = [], [], []
    for name, x in cases(held):
        y = C.render(x.astype(np.float32), FS, {C.NOISE: 0}, C.host(**CLEAR))
        X.append(x); Y.append(y); names.append(name)
        print('rendered', name, flush=True)
    np.savez(path, names=np.array(names), **{'x%d' % i: v for i, v in enumerate(X)}, **{'y%d' % i: v for i, v in enumerate(Y)})


def load(held=False):
    path = DATA.replace('.npz', '_held.npz') if held else DATA
    d = np.load(path)
    names = list(d['names'])
    h = json.load(open(PARAMS))['h']
    data = []
    for i in range(len(names)):
        u = pre(d['x%d' % i]); data.append((u, band(u, h), d['y%d' % i].astype(np.float64)))
    return names, data


def nulls(p, la, data):
    out = []
    for u, hu, y in data:
        z = model(u, hu, p, la)
        out.append(10 * np.log10(np.sum((y - z) ** 2) / np.sum(y ** 2) + 1e-30))
    return out


FREE = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19]


def fit():
    names, data = load()
    js = json.load(open(PARAMS))
    p0 = np.array(js.get('p', START), float)
    la = int(js.get('la', 24))
    print('start (la %d): mean %.2f' % (la, np.mean(nulls(p0, la, data))))
    best = [np.mean(nulls(p0, la, data)), p0.copy(), la]
    lo = {1: 0, 3: 0.2, 5: 0.2, 6: 0.01, 7: 0.01, 8: 0.1, 9: 1, 10: 0.1, 12: 0, 13: 0.001, 14: 0, 15: 0.1, 17: 0.1, 18: 0.1, 19: 0}
    hi = {19: 1.0, 2: 15000, 4: 8000}

    def run(p0, la):
        def obj(q):
            p = p0.copy(); p[FREE] = q
            for i, v in lo.items():
                if p[i] < v: return 1e3
            for i, v in hi.items():
                if p[i] > v: return 1e3
            v = float(np.mean(nulls(p, la, data)))
            if v < best[0]: best[0] = v; best[1] = p.copy(); best[2] = la
            return v
        minimize(obj, p0[FREE], method='Nelder-Mead', options=dict(maxiter=2500, xatol=1e-4, fatol=1e-3, adaptive=True))

    for rnd in range(3):
        for lin in (best[1][16], 1 - best[1][16]) if rnd == 0 else (best[1][16],):
            p = best[1].copy(); p[16] = lin
            run(p, best[2])
            print('round %d, release %s: mean %.2f' % (rnd, 'linear' if best[1][16] else 'dB', best[0]), flush=True)
        # the look-ahead
        for l2 in range(0, 64, 2):
            v = float(np.mean(nulls(best[1], l2, data)))
            if v < best[0]: best[0] = v; best[2] = l2
        print('  look-ahead %d: mean %.2f' % (best[2], best[0]), flush=True)
        js['p'] = [float(v) for v in best[1]]; js['la'] = best[2]; js['names'] = NAMES
        json.dump(js, open(PARAMS, 'w'), indent=1)
    p = best[1]
    print('best:', ' '.join('%s=%.4g' % (n, v) for n, v in zip(NAMES, p)), 'la', best[2])
    print('nulls:', '; '.join('%s %.1f' % (n, v) for n, v in zip(names, nulls(p, best[2], data))))


def check():
    js = json.load(open(PARAMS))
    for held in (False, True):
        names, data = load(held)
        ns = nulls(np.array(js['p']), js['la'], data)
        print('held-out' if held else 'training', 'mean %.2f:' % np.mean(ns), '; '.join('%s %.1f' % (n, v) for n, v in zip(names, ns)))
        # without the de-esser
        print('   without:', '; '.join('%.1f' % (10 * np.log10(np.sum((y - u) ** 2) / np.sum(y ** 2))) for u, hu, y in data))


def header():
    """writes plugins/WVoxChain/WVoxChainDeEss.h: the parameters, the look-ahead (samples at 48 kHz) and the high band's
    taps per rate (designed at 44.1 / 88.2 / 96 kHz from the 48 kHz band's response, whole above 20 kHz)"""
    js = json.load(open(PARAMS))
    h = np.array(js['h'])
    fgrid = np.linspace(0, 24000, 4801)
    H48 = h[0] + 2 * sum(h[k] * np.cos(2 * np.pi * fgrid * k / FS) for k in range(1, NT + 1))
    rows = []
    for fs in (44100, 48000, 88200, 96000):
        if fs == 48000: c = h
        else:
            nt = int(round(NT * fs / FS))
            f = np.linspace(0, fs / 2, 4001)
            target = np.where(f <= 20000, np.interp(f, fgrid, H48), 1.0)
            A = np.stack([np.ones(len(f))] + [2 * np.cos(2 * np.pi * f * k / fs) for k in range(1, nt + 1)], 1)
            c, *_ = np.linalg.lstsq(A, target, rcond=None)
            e = A @ c - target
            print('%d: %d taps, residual %.1f dB' % (fs, 2 * nt + 1, 10 * np.log10(np.sum(e ** 2) / np.sum(target ** 2))))
        rows.append('{ %d.0, %d, { %s } }' % (fs, len(c) - 1, ', '.join('%.17g' % v for v in c)))
    out = os.path.join(HERE, '..', 'plugins', 'WVoxChain', 'WVoxChainDeEss.h')
    lines = ['#pragma once', "// Generated by nulltest/fit_voxchain_deess.py from Phase 4's fit of the main de-esser: its parameters (deess.c's",
             '// order: ' + ' '.join(NAMES) + '),', "// the look-ahead in samples at 48 kHz, and the zero-phase high band's taps per rate (h[0] the",
             '// centre, h[k] = h[-k]). Do not edit.', 'namespace wvoxchain::deesstables', '{',
             'inline constexpr int nParams = %d;' % len(NAMES),
             'inline constexpr double p[nParams] = { %s };' % ', '.join('%.9g' % v for v in js['p']),
             'inline constexpr int lookAhead48 = %d;' % js['la'],
             'struct Taps { double fs; int nt; double h[49]; };',
             'inline constexpr Taps taps[4] = { %s };' % ', '.join(rows), '}', '']
    open(out, 'w').write('\n'.join(lines))
    print('wrote', out)


if __name__ == '__main__':
    for a in sys.argv[1:] or ['taps', 'render', 'fit', 'check']:
        if a == 'render_held': render(True)
        else: globals()[a]()
