"""Checks WVox Rider's riding model (vrmodel.c) against the reference renders (vr_ref.py) by a local fit: CMA-ES over
the continuous constants (times in log), started at the reference's hidden values (vrmodel.DEFAULTS). The objective is
the mean null in dB over the probes and the training corpus parts (0 .. 23); the held-out parts (24 .. 35) are only
reported. The result goes to measurements/wvoxrider/riding_fit.json; the plug-in keeps the hidden values unless the
fit finds a clear gain (it did not: MEASUREMENTS.md "WVox Rider: Phase 4").

    python -I vr_fit.py fit [gens]          the local fit
    python -I vr_fit.py report [fit]        the hidden values (or the saved fit) on probes / training / held-out
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from concurrent.futures import ThreadPoolExecutor
import vr_eval as E
import vrmodel as M
import vocal_corpus as V

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'wvoxrider', 'riding_fit.json')
# (name, log scale?, low, high); the starts are vrmodel.DEFAULTS
FREE = [('c', False, -3, 3), ('bias', False, -60, -30), ('fatt_s', True, 1, 500), ('fatt_f', True, 0.5, 200),
        ('frel', True, 50, 10000), ('fhold', True, 100, 50000), ('fpow', False, 0.5, 3.0), ('vatt', True, 0.001, 10),
        ('vrel', True, 0.001, 10), ('vk', False, 6, 22), ('ratt_s', True, 0.01, 50), ('ratt_f', True, 0.01, 50),
        ('rrel', True, 20, 10000), ('park', True, 100, 10000), ('phold_s', True, 50, 5000), ('phold_f', True, 5, 500),
        ('spill', False, -6, 6)]
POOL = ThreadPoolExecutor(8)


def unpack(q):
    p = {}
    for (n, lg, lo, hi), z in zip(FREE, q):
        v0 = M.DEFAULTS[n]
        v = np.exp(np.log(v0) + z) if lg else v0 + z * (abs(v0) if v0 else 1.0)
        p[n] = float(np.clip(v, lo, hi))
    return p


def load_cases(which):
    cs = []
    if 'probes' in which: cs += E.cases('probes')
    if 'train' in which: cs += E.cases('corpus', keys=[str(k) for k in V.TRAIN])
    if 'held' in which: cs += E.cases('corpus', keys=[str(k) for k in V.HELD])
    return cs


def nulls(params, cs):
    return np.array(list(POOL.map(lambda c: E.evaluate(params, c)[0], cs)))


def cma(f, x0, sigma, gens, lam=16, seed=1):
    rng = np.random.default_rng(seed)
    n = len(x0); mu = lam // 2
    w = np.log(mu + 0.5) - np.log(np.arange(1, mu + 1)); w /= w.sum(); mueff = 1 / np.sum(w ** 2)
    cc = (4 + mueff / n) / (n + 4 + 2 * mueff / n); cs_ = (mueff + 2) / (n + mueff + 5)
    c1 = 2 / ((n + 1.3) ** 2 + mueff); cmu = min(1 - c1, 2 * (mueff - 2 + 1 / mueff) / ((n + 2) ** 2 + mueff))
    damps = 1 + 2 * max(0, np.sqrt((mueff - 1) / (n + 1)) - 1) + cs_
    chiN = np.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    m = x0.copy(); C = np.eye(n); pc = np.zeros(n); ps = np.zeros(n); Bm = np.eye(n); D = np.ones(n)
    best = (f(m), m.copy()); print('start %.3f' % best[0], flush=True)
    for gen in range(gens):
        z = rng.standard_normal((lam, n)); y = z @ (Bm * D).T; X = m + sigma * y
        F = np.array([f(x) for x in X]); idx = np.argsort(F)
        if F[idx[0]] < best[0]: best = (F[idx[0]], X[idx[0]].copy())
        yw = w @ y[idx[:mu]]; m = m + sigma * yw
        Cinvh = Bm @ np.diag(1 / D) @ Bm.T
        ps = (1 - cs_) * ps + np.sqrt(cs_ * (2 - cs_) * mueff) * (Cinvh @ yw)
        hs = np.linalg.norm(ps) / np.sqrt(1 - (1 - cs_) ** (2 * (gen + 1))) < (1.4 + 2 / (n + 1)) * chiN
        pc = (1 - cc) * pc + hs * np.sqrt(cc * (2 - cc) * mueff) * yw
        C = (1 - c1 - cmu) * C + c1 * (np.outer(pc, pc) + (1 - hs) * cc * (2 - cc) * C) + cmu * (y[idx[:mu]].T * w) @ y[idx[:mu]]
        sigma *= np.exp((cs_ / damps) * (np.linalg.norm(ps) / chiN - 1))
        C = (C + C.T) / 2; D2, Bm = np.linalg.eigh(C); D = np.sqrt(np.maximum(D2, 1e-20))
        if gen % 5 == 0 or gen == gens - 1:
            print('gen %3d best %.3f  generation best %.3f  median %.3f  sigma %.3f' % (gen, best[0], F[idx[0]], np.median(F), sigma), flush=True)
    return best


def fit(gens):
    cs = load_cases(('probes', 'train'))
    print('%d cases' % len(cs), flush=True)
    f = lambda q: float(np.mean(nulls(unpack(q), cs)))
    c, q = cma(f, np.zeros(len(FREE)), 0.05, gens)
    p = unpack(q)
    c0 = f(np.zeros(len(FREE)))
    print('hidden values %.3f, fit %.3f: %s' % (c0, c, json.dumps({k: round(v, 4) for k, v in p.items()})), flush=True)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(dict(cost_hidden=c0, cost=c, params=p), open(OUT, 'w'), indent=1)
    print('saved', OUT)


def load_fit():
    return json.load(open(OUT))['params'] if os.path.exists(OUT) else {}


def report(use_fit=False):
    p = load_fit() if use_fit else {}
    for which in ('probes', 'train', 'held'):
        cs = load_cases((which,))
        n = nulls(p, cs)
        by = {}
        for c, v in zip(cs, n): by.setdefault(c['setting'], []).append(v)
        print('%-6s median %+6.1f mean %+6.1f worst %+6.1f | %s' % (which, np.median(n), np.mean(n), n.max(),
              ' '.join('%s %+.1f' % (s, np.median(v)) for s, v in by.items())))


if __name__ == '__main__':
    if sys.argv[1] == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 60)
    else: report(len(sys.argv) > 2 and sys.argv[2] == 'fit')
