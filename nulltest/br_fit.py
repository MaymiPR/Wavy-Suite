"""Fits the note detector of WBass Rider's riding model (brmodel.c) to the reference's probe set (br_probes.py) and its
gain traces on musical material (br_traces.py): CMA-ES on the sum over probes and read times of |model gain -
reference gain| (clipped at 2 dB) plus 0.1 x the same over the traces' 10 ms blocks, plus CW x the same over the bass
corpus's training parts at every recorded setting (bass_corpus.py, br_corpus_ref.py); prints the mismatches.

    python -I br_fit.py [generations [sigma]]  writes measurements/wbassrider/detector_fit.json
    python -I br_fit.py check              the current fit's mismatches, probe by probe
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from concurrent.futures import ThreadPoolExecutor
import brmodel as B
import br_probes as PR
import br_traces as TR

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'detector_fit.json')
SETTINGS = [(r, s) for r in ('Slow', 'Fast') for s in (0.0, 25.0, 50.0, 75.0, 100.0)]
# the free parameters: (name, start, transform) with 'log' kept positive
GLOBAL = [('slow_att', 30.0, 'log'), ('slow_rel', 1000.0, 'log'), ('ref_rel', 5000.0, 'log'), ('hyst', 0.3, 'log'),
          ('rel_ms', 21.0, 'log'), ('settle', 20.0, 'log'), ('off_ms', 60.0, 'log'), ('win_ms', 75.0, 'log'), ('off_rel_ms', 60.0, 'log'),
          ('f_att', 5.0, 'log'), ('f_rel', 150.0, 'log'), ('pitch_lp', 200.0, 'log'), ('rr_lo1', 39.0, 'log'), ('rr_hi1', 49.0, 'log')]
# model options held fixed: at Response Fast a held note at a pitch in the band rr_lo1 .. rr_hi1 is ridden afresh once it is
# RR_DEPTH under its slow envelope
FIXED = dict(rr_band=7.0, rr_lo2=0.0, rr_hi2=0.0, block_rise=0.0, gate_block=2.0, hold_latch=2.0)
RR_DEPTH, RR_SENS = 4.4, 37.5   # its depth under S (measured: 4.3 .. 4.8 dB at any Sensitivity), from Sensitivity 50 (not at 0 / 25)
SENS = (0.0, 25.0, 50.0, 75.0, 100.0)
THR0 = {0.0: 1.25, 25.0: 1.25, 50.0: 1.25, 75.0: 1.0, 100.0: 0.5}
MARGIN0 = {0.0: 0.25, 25.0: 0.0, 50.0: -0.25, 75.0: -1.25, 100.0: -1.75}   # the attack phase: N over its follower + margin
RISE0 = {0.0: 3.5, 25.0: 3.0, 50.0: 2.5, 75.0: 2.0, 100.0: 1.5}            # a released note's rise that is a new note
DEPTH0 = {('Slow', 0.0): 12.4, ('Slow', 25.0): 12.4, ('Slow', 50.0): 12.4, ('Slow', 75.0): 1.5, ('Slow', 100.0): 0.5,
          ('Fast', 0.0): 4.5, ('Fast', 25.0): 3.0, ('Fast', 50.0): 1.5, ('Fast', 75.0): 1.0, ('Fast', 100.0): 0.5}
PROBES = PR.load()
SIGS = [PR.build(p['recipe']) for p in PROBES]
TRACES = TR.load()
TSIGS = {n: TR.material(n) for n in set(t['name'] for t in TRACES)}
TW = 0.1
# the bass corpus's training parts at every recorded setting, as 10 ms blocks (br_corpus_ref.py)
import bass_corpus as KC, br_corpus_ref as CR
CORPUS = []
for _k in KC.TRAIN:
    _x = KC.part(_k)[0]
    for _s in CR.SETTINGS:
        if os.path.exists(CR.path(_k, _s)):
            _i, _v = TR.blocks(_x, CR.load(_k, _s)); CORPUS.append(dict(k=_k, name=_s, settings=CR.SETTINGS[_s], idx=_i, ref=_v))
    TSIGS['corpus_%d' % _k] = _x
CW = 0.01


def setting_params(td, resp, sens):
    thr, depth, margin, rise = td
    return dict(rel_depth=float(np.interp(sens, [s for r, s in SETTINGS if r == resp], [depth[(r, s)] for r, s in SETTINGS if r == resp])),
                thr_u=float(np.interp(sens, SENS, [thr[k] for k in SENS])), thr_a=float(np.interp(sens, SENS, [margin[k] for k in SENS])),
                on_rise=float(np.interp(sens, SENS, [rise[k] for k in SENS])),
                rr_depth=RR_DEPTH if sens >= RR_SENS else 1.0e3)


def unpack(q):
    g = dict(FIXED)
    for (name, _, tr), v in zip(GLOBAL, q[:len(GLOBAL)]): g[name] = float(np.exp(v)) if tr == 'log' else float(v)
    n0 = len(GLOBAL); n1 = n0 + len(SENS); n2 = n1 + len(SENS); n3 = n2 + len(SENS)
    thr = {k: float(v) for k, v in zip(SENS, q[n0:n1])}
    margin = {k: float(v) for k, v in zip(SENS, q[n1:n2])}
    rise = {k: float(np.exp(v)) for k, v in zip(SENS, q[n2:n3])}
    depth = {k: float(np.exp(v)) for k, v in zip(SETTINGS, q[n3:])}
    return g, (thr, depth, margin, rise)


def pack(g, td):
    thr, depth, margin, rise = td
    return np.array([np.log(g[n]) if tr == 'log' else g[n] for n, _, tr in GLOBAL] + [thr[k] for k in SENS] + [margin[k] for k in SENS]
                    + [np.log(rise[k]) for k in SENS] + [np.log(depth[k]) for k in SETTINGS])


def model_gains(i, g, depth):
    p = PROBES[i]; st = p['settings']
    resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
    gm = B.ride(SIGS[i], artifacts=float(st.get('artifacts', 50.0)), sensitivity=sens, response=resp, target=float(st.get('target', -14.0)),
                **dict(g, **setting_params(depth, resp, sens)))
    w = 480 if len(p['read']) > 3 else 2400
    return np.array([np.median(gm[int(t * B.FS):int(t * B.FS) + w]) for t in p['read']])


def corpus_gains(j, g, depth):
    t = CORPUS[j]; st = t['settings']
    resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
    gm = B.ride(TSIGS['corpus_%d' % t['k']], artifacts=float(st.get('artifacts', 50.0)), sensitivity=sens, response=resp,
                target=float(st.get('target', -14.0)), **dict(g, **setting_params(depth, resp, sens)))
    h = TR.BLOCK // 2
    return np.array([np.median(gm[c - h:c + h]) for c in t['idx']])


def trace_gains(j, g, depth):
    t = TRACES[j]; st = t['settings']
    resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
    gm = B.ride(TSIGS[t['name']], artifacts=float(st.get('artifacts', 50.0)), sensitivity=sens, response=resp,
                **dict(g, **setting_params(depth, resp, sens)))
    h = TR.BLOCK // 2
    return np.array([np.median(gm[c - h:c + h]) for c in t['idx']])


POOL = ThreadPoolExecutor(16)


def cost(q, detail=False):
    g, depth = unpack(q)
    res = list(POOL.map(lambda i: model_gains(i, g, depth), range(len(PROBES))))
    tres = list(POOL.map(lambda j: trace_gains(j, g, depth), range(len(TRACES))))
    errs = [np.abs(r - p['ref']) for r, p in zip(res, PROBES)]
    terrs = [np.abs(r - t['ref']) for r, t in zip(tres, TRACES)]
    cres = list(POOL.map(lambda j: corpus_gains(j, g, depth), range(len(CORPUS))))
    cerrs = [np.abs(r - t['ref']) for r, t in zip(cres, CORPUS)]
    c = float(sum(np.minimum(e, 2.0).sum() for e in errs)) + TW * float(sum(np.minimum(e, 2.0).sum() for e in terrs))         + CW * float(sum(np.minimum(e, 2.0).sum() for e in cerrs))
    return (c, res, errs, terrs, cerrs) if detail else c


def cma(f, x0, sigma, gens, lam=16, seed=1):
    rng = np.random.default_rng(seed)
    n = len(x0); mu = lam // 2
    w = np.log(mu + 0.5) - np.log(np.arange(1, mu + 1)); w /= w.sum(); mueff = 1 / np.sum(w ** 2)
    cc = (4 + mueff / n) / (n + 4 + 2 * mueff / n); cs_ = (mueff + 2) / (n + mueff + 5)
    c1 = 2 / ((n + 1.3) ** 2 + mueff); cmu = min(1 - c1, 2 * (mueff - 2 + 1 / mueff) / ((n + 2) ** 2 + mueff))
    damps = 1 + 2 * max(0, np.sqrt((mueff - 1) / (n + 1)) - 1) + cs_
    chiN = np.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    m = x0.copy(); C = np.eye(n); pc = np.zeros(n); ps = np.zeros(n); Bm = np.eye(n); D = np.ones(n)
    best = (f(m), m.copy()); print('start %.2f' % best[0], flush=True)
    for gen in range(gens):
        z = rng.standard_normal((lam, n)); y = z @ (Bm * D).T; X = m + sigma * y
        F = np.array([f(x) for x in X]); idx = np.argsort(F)
        if F[idx[0]] < best[0]: best = (F[idx[0]], X[idx[0]].copy()); save(best[1], best[0])
        yw = w @ y[idx[:mu]]; m = m + sigma * yw
        Cinvh = Bm @ np.diag(1 / D) @ Bm.T
        ps = (1 - cs_) * ps + np.sqrt(cs_ * (2 - cs_) * mueff) * (Cinvh @ yw)
        hs = np.linalg.norm(ps) / np.sqrt(1 - (1 - cs_) ** (2 * (gen + 1))) < (1.4 + 2 / (n + 1)) * chiN
        pc = (1 - cc) * pc + hs * np.sqrt(cc * (2 - cc) * mueff) * yw
        C = (1 - c1 - cmu) * C + c1 * (np.outer(pc, pc) + (1 - hs) * cc * (2 - cc) * C) + cmu * (y[idx[:mu]].T * w) @ y[idx[:mu]]
        sigma *= np.exp((cs_ / damps) * (np.linalg.norm(ps) / chiN - 1))
        C = (C + C.T) / 2; D2, Bm = np.linalg.eigh(C); D = np.sqrt(np.maximum(D2, 1e-20))
        print('gen %3d best %.2f  generation best %.2f  median %.2f  sigma %.3f' % (gen, best[0], F[idx[0]], np.median(F), sigma), flush=True)
    return best


def save(q, c):
    g, (thr, depth, margin, rise) = unpack(q)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(dict(cost=c, globals=g, thr_u={'%g' % k: v for k, v in thr.items()}, margin={'%g' % k: v for k, v in margin.items()},
                   on_rise={'%g' % k: v for k, v in rise.items()},
                   rel_depth={'%s %g' % k: v for k, v in depth.items()}), open(OUT, 'w'), indent=1)


def load_fit():
    if not os.path.exists(OUT): return dict(FIXED, **{n: v for n, v, _ in GLOBAL}), (dict(THR0), dict(DEPTH0), dict(MARGIN0), dict(RISE0))
    d = json.load(open(OUT))
    g = dict(FIXED, **{n: d['globals'].get(n, v) for n, v, _ in GLOBAL})
    if 'pitch_lp' not in d['globals']: g['pitch_lp'] = 0.0      # (a fit from before the pitch low-pass: off)
    if 'rr_lo1' not in d['globals']: g['rr_band'] = 0.0         # (a fit from before the band re-ride: off)
    thr = {float(k): v for k, v in d['thr_u'].items()} if 'thr_u' in d else dict(THR0)
    depth = {(k.split()[0], float(k.split()[1])): v for k, v in d['rel_depth'].items()} if 'rel_depth' in d else dict(DEPTH0)
    margin = {float(k): v for k, v in d['margin'].items()} if 'margin' in d else {k: d['globals'].get('thr_a', 0.3) for k in SENS}
    # (a fit from before the per-Sensitivity rise: its one value everywhere)
    rise = {float(k): v for k, v in d['on_rise'].items()} if 'on_rise' in d else {k: d['globals'].get('on_rise', 1.0) for k in SENS}
    return g, (thr, depth, margin, rise)


def check():
    g, depth = load_fit()
    c, res, errs, terrs, cerrs = cost(pack(g, depth), detail=True)
    bad = 0
    for p, r, e in zip(PROBES, res, errs):
        if e.max() > 0.3:
            bad += 1
            print('%-60s %-40s ref %s model %s' % (json.dumps(p['recipe'])[:60], json.dumps(p['settings']), np.round(p['ref'], 2), np.round(r, 2)))
    print('cost %.2f; %d of %d probes off by more than 0.3 dB' % (c, bad, len(PROBES)))
    for t, e in zip(TRACES, terrs):
        print('  trace %-10s %-28s median |d| %.3f  90%% %.3f  share > 1 dB %.3f' % (t['name'], json.dumps(t['settings']), np.median(e), np.percentile(e, 90), np.mean(e > 1)))
    print(json.dumps(g), json.dumps({'%g' % k: round(v, 3) for k, v in depth[0].items()}), json.dumps({'%s %g' % k: round(v, 3) for k, v in depth[1].items()}),
          json.dumps({'%g' % k: round(v, 3) for k, v in depth[2].items()}), json.dumps({'%g' % k: round(v, 3) for k, v in depth[3].items()}))
    for name in CR.SETTINGS:
        e = np.concatenate([ee for ee, t in zip(cerrs, CORPUS) if t['name'] == name] or [np.zeros(1)])
        print('  corpus %-8s median |d| %.3f  90%% %.3f  share > 1 dB %.3f' % (name, np.median(e), np.percentile(e, 90), np.mean(e > 1)))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'check': check()
    else:
        g, depth = load_fit()
        best = cma(cost, pack(g, depth), float(sys.argv[2]) if len(sys.argv) > 2 else 0.3, int(sys.argv[1]) if len(sys.argv) > 1 else 40)
        save(best[1], best[0]); check()
