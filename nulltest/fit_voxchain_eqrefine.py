"""Phase 4: a second pass over WVoxChainEqTables.h for the positions fit_voxchain_eqtables.py left above -85 dB (Bass
Upper at fader 0 and up, some Bass Sub / Lower positions at 88.2 / 96 kHz). Their filters have zeros on or near the
unit circle (Bass Upper's subsonic high-pass notches about 10 Hz) and poles clustered near DC, which Steiglitz-McBride's
direct form cannot resolve. Here each is refined with its poles and zeros as the parameters (complex pairs as radius
and angle, poles kept inside the unit circle), least squares on the response over the clear chain's (the fitted
filter times the clear chain's spectrum against the position's), from several starts: the current table's sections
and Steiglitz-McBride fits of a few orders. The best is kept if it beats the table; the header is rewritten.

    python -I fit_voxchain_eqrefine.py [--workers=14] [--limit=-85] [--skip=LOG:N]  (skip the first N positions an
                                                                             earlier run's log reports: already written)
"""
import json
import os
import re
import sys
import time
import numpy as np
from multiprocessing import Pool
from scipy.optimize import least_squares
from scipy.signal import lfilter, sosfilt, zpk2sos

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fit_voxchain_eqtables as E

HDR = E.OUT
ERRS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'eqtables_errors.json')


def read_header():
    txt = open(HDR).read()
    nsec = int(re.search(r'sections = (\d+);', txt).group(1))
    out = {}
    for m in re.finditer(r'Position (bass|treble)(\d)_(\d+)\[201\] = \{(.*?)\};', txt):
        v = np.array([float(t) for t in re.findall(r'[-+]?\d[\d.eE+-]*', m.group(4))]).reshape(201, nsec, 5)
        out[(int(m.group(3)), m.group(1), int(m.group(2)))] = v
    return nsec, out


def sos_roots(S):
    z, p, k = [], [], 1.0
    for s in S:
        b = s[:3]; a = np.array([1.0, s[3], s[4]])
        if np.allclose(b, [1, 0, 0]) and np.allclose(a, [1, 0, 0]): continue
        bt = np.trim_zeros(b, 'b'); at = np.trim_zeros(a, 'b')
        k *= bt[0]
        z += list(np.roots(bt)); p += list(np.roots(at))
    return np.array(z), np.array(p), k


def to_theta(z, p, k):
    cz = [r for r in z if r.imag > 1e-12]; rz = [r.real for r in z if abs(r.imag) <= 1e-12]
    p = np.where(np.abs(p) >= 0.9999999, p / np.abs(p) * 0.9999999, p)
    cp = [r for r in p if r.imag > 1e-12]; rp = [r.real for r in p if abs(r.imag) <= 1e-12]
    th = []
    for r in cz: th += [abs(r), np.angle(r)]
    th += rz
    for r in cp: th += [np.log(1 - abs(r)), np.angle(r)]
    th += [np.arctanh(np.clip(v, -0.9999999, 0.9999999)) for v in rp]
    th += [k]
    return np.array(th, float), (len(cz), len(rz), len(cp), len(rp))


def from_theta(t, dims):
    ncz, nrz, ncp, nrp = dims
    i = 0; z = []; p = []
    for _ in range(ncz): r = t[i] * np.exp(1j * t[i + 1]); i += 2; z += [r, np.conj(r)]
    for _ in range(nrz): z.append(t[i]); i += 1
    for _ in range(ncp): r = (1 - np.exp(t[i])) * np.exp(1j * t[i + 1]); i += 2; p += [r, np.conj(r)]
    for _ in range(nrp): p.append(np.tanh(t[i])); i += 1
    return np.array(z), np.array(p), t[i]


def response(z, p, k, w):
    e = np.exp(-1j * w)
    num = np.ones_like(e) * k
    for r in z: num = num * (1 - r * e)
    den = np.ones_like(e)
    for r in p: den = den * (1 - r * e)
    return num / den


def jacobian(t, dims, w, H0):
    """d (response x H0) / d theta, analytically (each factor's derivative over itself, times the response)"""
    ncz, nrz, ncp, nrp = dims
    e = np.exp(-1j * w)
    z, p, k = from_theta(t, dims)
    P = np.ones_like(e)
    for r in z: P = P * (1 - r * e)
    for r in p: P = P / (1 - r * e)
    G = k * P * H0
    cols = []
    i = 0
    for _ in range(ncz):
        m, a = t[i], t[i + 1]; i += 2
        r = m * np.exp(1j * a); u, v = 1 - r * e, 1 - np.conj(r) * e
        cols.append(G * (-np.exp(1j * a) * e / u - np.exp(-1j * a) * e / v))
        cols.append(G * (-1j * r * e / u + 1j * np.conj(r) * e / v))
    for _ in range(nrz):
        r = t[i]; i += 1
        cols.append(G * (-e / (1 - r * e)))
    for _ in range(ncp):
        s, a = t[i], t[i + 1]; i += 2
        m = 1 - np.exp(s); r = m * np.exp(1j * a); u, v = 1 - r * e, 1 - np.conj(r) * e
        cols.append(G * (np.exp(1j * a) * e / u + np.exp(-1j * a) * e / v) * (-np.exp(s)))
        cols.append(G * (1j * r * e / u - 1j * np.conj(r) * e / v))
    for _ in range(nrp):
        q = np.tanh(t[i]); i += 1
        cols.append(G * (e / (1 - q * e)) * (1 - q * q))
    cols.append(P * H0)
    J = np.stack(cols, 1)
    return np.concatenate([J.real, J.imag])


_C = None
_h0 = {}


def _init():
    global _C
    import cv_ref as C
    _C = C
    C.plugin()
    E._C = C


def job(j):
    fs, st, col, k, S = j
    N = 8192 * (2 if fs > 48000 else 1)                              # (the responses are below -170 dB by then)
    if fs not in _h0: _h0[fs] = E.capture(E.CLEAR, fs)[:N]
    h0 = _h0[fs]
    v = round(-10 + 0.1 * k, 1)
    y = E.capture(dict(E.CLEAR, **{st + '_color': col, st: v}), fs)[:N]
    H0 = np.fft.rfft(h0); Y = np.fft.rfft(y); w = 2 * np.pi * np.arange(len(Y)) / N
    ey = np.sum(y ** 2)

    def terr(z, p, kk):
        try: s = zpk2sos(z, p, kk)
        except Exception: return 0.0, None
        return 10 * np.log10(np.sum((sosfilt(s, h0) - y) ** 2) / ey + 1e-30), s

    starts = [sos_roots(S)]
    for o in ((16, 16), (20, 18), (14, 12), (12, 10)):
        try:
            b, a = E.stmcb(y, h0, *o, it=30)
            starts.append((np.roots(b), np.roots(a), b[0]))
        except Exception:
            pass
    best = terr(*starts[0]) + (len(starts[0][1]),)
    first = best[0]
    for z, p, kk in starts:
        if len(p) and np.abs(p).max() >= 1.0: p = np.where(np.abs(p) >= 1.0, 0.9999 / np.conj(p), p)
        th, dims = to_theta(z, p, kk)

        def res(t):
            r = response(*from_theta(t, dims), w) * H0 - Y
            return np.concatenate([r.real, r.imag])
        try:
            with np.errstate(all='ignore'):
                sol = least_squares(res, th, jac=lambda t: jacobian(t, dims, w, H0), method='lm', max_nfev=4000)
        except Exception:
            continue
        e = terr(*from_theta(sol.x, dims))
        if e[1] is not None and e[0] < best[0]: best = e + (len(p),)
        if best[0] < -88: break
    return (fs, st, col, k, first, best[0], best[1])


def write(nsec_old, tables, errs):
    nsec = max(max(len(s) for s in pos) for pos in tables.values())
    lines = ['#pragma once', '// Generated by nulltest/fit_voxchain_eqtables.py (and refined by fit_voxchain_eqrefine.py): Bass and Treble at each of',
             '// the fader\'s 201 positions per colour and rate, as second-order sections (b0 b1 b2 a1 a2, a0 = 1; unused sections',
             '// pass), fitted to the reference\'s impulse responses through the chain with every colour clear. Do not edit.',
             'namespace wvoxchain::eqtables', '{', 'inline constexpr int sections = %d;' % nsec,
             'struct Position { double sos[%d][5]; };' % nsec, 'struct Rate { double fs; const Position* bass[3]; const Position* treble[3]; };']
    for fs in E.RATES:
        for st in E.STAGES:
            for col in (1, 2, 3):
                body = []
                for S in tables[(fs, st, col)]:
                    S = [list(s) for s in S]
                    while len(S) < nsec: S.append([1.0, 0.0, 0.0, 0.0, 0.0])
                    body.append('{ { %s } }' % ', '.join('{ %s }' % ', '.join('%.17g' % v for v in s) for s in S))
                lines.append('inline constexpr Position %s%d_%d[201] = { %s };' % (st, col, fs, ', '.join(body)))
    lines.append('inline constexpr Rate rates[] = { %s };' % ', '.join(
        '{ %d.0, { &bass1_%d[0], &bass2_%d[0], &bass3_%d[0] }, { &treble1_%d[0], &treble2_%d[0], &treble3_%d[0] } }' % ((fs,) * 7) for fs in E.RATES))
    lines += ['}', '']
    open(HDR, 'w').write('\n'.join(lines))
    json.dump(errs, open(ERRS, 'w'))
    print('wrote %s (%d sections, %d kB)' % (HDR, nsec, os.path.getsize(HDR) // 1024), flush=True)


def main():
    workers, limit, skip = 14, -85.0, set()
    for a in sys.argv[1:]:
        if a.startswith('--workers='): workers = int(a.split('=')[1])
        if a.startswith('--limit='): limit = float(a.split('=')[1])
        if a.startswith('--skip='):
            path, count = a.split('=', 1)[1].rsplit(':', 1)
            done = [l for l in open(path) if re.match(r'\s*\d+ (bass|treble) c\d', l)][:int(count)]
            for l in done:
                m = re.match(r'\s*(\d+) (bass|treble) c(\d)\s+([-+]\d+\.\d):', l)
                skip.add((int(m.group(1)), m.group(2), int(m.group(3)), int(round((float(m.group(4)) + 10) * 10))))
    nsec, raw = read_header()
    tables = {key: [[s for s in pos if not (np.allclose(s[:3], [1, 0, 0]) and np.allclose(s[3:], [0, 0]))] or [np.array([1.0, 0, 0, 0, 0])]
                    for pos in v] for key, v in raw.items()}
    errs = json.load(open(ERRS))
    idx = {(e[0], e[1], e[2], e[3]): i for i, e in enumerate(errs)}
    jobs = [(e[0], e[1], e[2], e[3], raw[(e[0], e[1], e[2])][e[3]]) for e in errs if e[4] > limit and tuple(e[:4]) not in skip]
    order = {48000: 0, 44100: 1, 96000: 2, 88200: 3}
    jobs.sort(key=lambda j: (order[j[0]], -errs[idx[j[:4]]][4]))           # the common rates, then the worst first
    print('%d positions above %.0f dB' % (len(jobs), limit), flush=True)
    t0 = time.time(); done = 0
    with Pool(workers, initializer=_init) as pool:
        for fs, st, col, k, first, e, s in pool.imap_unordered(job, jobs):
            done += 1
            if s is not None and e < errs[idx[(fs, st, col, k)]][4] - 0.5:
                tables[(fs, st, col)][k] = [[r[0] / r[3], r[1] / r[3], r[2] / r[3], r[4] / r[3], r[5] / r[3]] for r in s]
                errs[idx[(fs, st, col, k)]][4] = float(e)
            print('%6d %s c%d %+5.1f: %.1f -> %.1f dB  (%d / %d, %.0f s)' % (fs, st, col, -10 + 0.1 * k, first, e, done, len(jobs), time.time() - t0), flush=True)
            if done % 25 == 0: write(nsec, tables, errs)
    write(nsec, tables, errs)


if __name__ == '__main__':
    main()
