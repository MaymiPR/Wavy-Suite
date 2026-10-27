"""Polishes br_fit.py's fit one per-setting value at a time: each release depth (Response x Sensitivity), new-note
threshold and attack margin (Sensitivity) on a grid around its value, keeping the grid point of least cost (the
probes and traces it changes). The narrow valleys between the detector's thresholds (a note released or not) are
what the CMA-ES steps over.

    python -I br_polish.py [passes] [globals[=a,b]]   updates measurements/wbassrider/detector_fit.json (globals: the
                                                       global values too, or those named, on the whole cost)
A value moves only for a gain over MIN_GAIN (smaller gains are noise that costs held-out material).
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import br_fit as F


MIN_GAIN = 0.2


def items(td, key):
    """the probes and traces whose setting parameters a change of key can move"""
    kind, k = key
    def hit(st):
        resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
        if kind == 'depth':
            return resp == k[0] and abs(sens - k[1]) < 25.0
        return abs(sens - k) < 25.0
    return ([i for i, p in enumerate(F.PROBES) if hit(p['settings'])], [j for j, t in enumerate(F.TRACES) if hit(t['settings'])],
            [j for j, t in enumerate(F.CORPUS) if hit(t['settings'])])


def part_cost(g, td, PI, TI, CI):
    pe = list(F.POOL.map(lambda i: np.minimum(np.abs(F.model_gains(i, g, td) - F.PROBES[i]['ref']), 2.0).sum(), PI))
    te = list(F.POOL.map(lambda j: np.minimum(np.abs(F.trace_gains(j, g, td) - F.TRACES[j]['ref']), 2.0).sum(), TI))
    ce = list(F.POOL.map(lambda j: np.minimum(np.abs(F.corpus_gains(j, g, td) - F.CORPUS[j]['ref']), 2.0).sum(), CI))
    return float(sum(pe)) + F.TW * float(sum(te)) + F.CW * float(sum(ce))


def grid(kind, v, name=None):
    if name in ('rr_lo1', 'rr_hi1'): return sorted(set([v] + [v + d for d in (-3.0, -2.0, -1.0, -0.5, 0.5, 1.0, 2.0, 3.0)]))
    if kind in ('depth', 'global', 'rise'): return sorted(set([v] + [v * f for f in (0.5, 0.7, 0.8, 0.9, 0.95, 1.05, 1.1, 1.25, 1.5, 2.0)]))
    return sorted(set([v] + [v + d for d in (-1.0, -0.5, -0.25, -0.1, -0.05, 0.05, 0.1, 0.25, 0.5, 1.0)]))


def main(passes, with_globals=False, names=None):
    g, td = F.load_fit()
    thr, depth, margin, rise = td
    keys = ([('global', n) for n, _, _ in F.GLOBAL if not names or n in names] if with_globals else []) + [('depth', k) for k in F.SETTINGS]         + [('thr_u', s) for s in F.SENS] + [('margin', s) for s in F.SENS] + [('rise', s) for s in F.SENS]
    ALL = (list(range(len(F.PROBES))), list(range(len(F.TRACES))), list(range(len(F.CORPUS))))
    for n in range(passes):
        moved = 0
        for key in keys:
            kind, k = key
            table = {'depth': depth, 'thr_u': thr, 'margin': margin, 'rise': rise, 'global': g}[kind]
            PI, TI, CI = ALL if kind == 'global' else items(td, key)
            v0 = table[k]; base = part_cost(g, td, PI, TI, CI); best = (base, v0)
            for v in grid(kind, v0, k if kind == 'global' else None):
                if v == v0: continue
                table[k] = v; c = part_cost(g, td, PI, TI, CI)
                if c < best[0] - (MIN_GAIN if best[1] == v0 else 1e-6): best = (c, v)
            table[k] = best[1]
            if best[1] != v0:
                moved += 1
                print('pass %d %-6s %-14s %8.3f -> %8.3f   cost of its items %.2f -> %.2f' % (n, kind, k, v0, best[1], base, best[0]), flush=True)
        total = F.cost(F.pack(g, td))
        F.save(F.pack(g, td), total)
        print('pass %d: %d moved, total cost %.2f' % (n, moved, total), flush=True)
        if not moved: break


if __name__ == '__main__':
    a = sys.argv[2] if len(sys.argv) > 2 else ''
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 3, a.startswith('globals'), a.split('=')[1].split(',') if '=' in a else None)
