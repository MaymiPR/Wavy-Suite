"""Fit the WVox slow path on deterministic probes of the reference: pulse trains, DC steps and releases
across Compression settings (private; reference readings are cached in measurements/rvox/struct_ref.npz).

    python -I fit_struct.py maxiter keys start [extra key=value ...]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import minimize
import nulltest_wvox as N

FS = 48000
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'struct_ref.npz')


def probes():
    P = []   # (name, signal, comp, read indices)
    for W in (1, 8, 48, 240):
        for per_ms in (5, 10, 20, 50):
            n = int(1.8 * FS); x = np.full(n, 0.01); per = int(per_ms * 48)
            for s0 in range(0, n, per): x[s0:s0 + W] = 0.316
            k = (int(1.5 * FS) // per) * per
            idx = [k + W + (per - W) // 4, k + W + (per - W) // 2, k + per - 20]
            P.append((f'pulses W{W} P{per_ms}', x.astype(np.float32), -24, idx))
    for L in (-10, -20):
        for d in (5, 10, 20, 50, 100, 200, 400):
            x = np.concatenate([np.full(int(0.5 * FS), 0.01), np.full(int(d * 48), 10 ** (L / 20)), np.full(int(1.3 * FS), 0.01)]).astype(np.float32)
            e = int(0.5 * FS) + int(d * 48)
            P.append((f'dc L{L} d{d}', x, -24, [e + int(t * 48) for t in (2, 20, 100, 400, 1000)]))
    for c in (-6, -12, -24, -36):
        x = np.concatenate([np.full(FS, 0.316), np.zeros(int(1.7 * FS))]).astype(np.float32)
        x[-int(0.1 * FS):] = 0.001
        P.append((f'release c{c}', x, c, [FS + int(t * 48) for t in (100, 400, 800, 1600)]))
    # x is zero during part of the release probe: read the level from a gain measured on a quiet probe tone instead
    return P


def level_db(y, x, idx, c):
    M = 10 ** (-c / 20); out = []
    for i in idx:
        if abs(x[i]) < 1e-6: out.append(np.nan); continue
        g = abs(y[i] / x[i]); out.append(20 * np.log10(max((0.99 * M / g - 1) / (M - 1), 1e-9)))
    return np.array(out)


def release_probe_reads(x, n_gap):
    return x


PROBES = probes()
# release probes: replace the zero gap by a -60 dB floor so the level can be read (it releases the same way, see MEASUREMENTS)
for i, (nm, x, c, idx) in enumerate(PROBES):
    if nm.startswith('release'):
        x = x.copy(); x[FS:] = np.where(x[FS:] == 0, 0.001, x[FS:]); PROBES[i] = (nm, x, c, idx)

if os.path.exists(CACHE):
    REF = list(np.load(CACHE)['ref'])
else:
    import refhost as R
    REF = [level_db(R.render(x, FS, comp=c)[0], x, idx, c) for nm, x, c, idx in PROBES]
    np.savez(CACHE, ref=np.array(REF, dtype=object), allow_pickle=True) if False else np.savez(CACHE, ref=np.array([np.pad(r, (0, 5 - len(r)), constant_values=np.nan) for r in REF]))
    REF = list(np.load(CACHE)['ref'])


def score(p, keys, extra, verbose=False):
    if min(p) < 0.05 or max(p) > 5000: return 1e3
    tune = tuple(f'{k}={v:.5g}' for k, v in zip(keys, p)) + tuple(extra)
    errs = []
    for (nm, x, c, idx), r in zip(PROBES, REF):
        o = level_db(N.ours(x, FS, c, None, 0, tune)[0], x, idx, c)
        e = (o - r[:len(o)]); e = e[np.isfinite(e)]
        errs.append(e)
        if verbose: print('%-18s ref %s ours %s' % (nm, np.round(r[:len(o)], 1), np.round(o, 1)))
    e = np.concatenate(errs)
    return float(np.sqrt(np.mean(e ** 2)))


if __name__ == '__main__':
    maxiter = int(sys.argv[1]); keys = sys.argv[2].split(','); p0 = [float(v) for v in sys.argv[3].split(',')]; extra = sys.argv[4:]
    print('start', score(p0, keys, extra), flush=True)
    r = minimize(score, p0, args=(keys, extra), method='Nelder-Mead', options={'maxiter': maxiter, 'xatol': 1e-3, 'fatol': 1e-3, 'adaptive': True})
    print('best', ','.join('%.5g' % v for v in r.x), r.fun, flush=True)
    score(r.x, keys, extra, True)
