"""Polishes br_fit.py's fit one per-setting value at a time: each release depth (Response x Sensitivity), new-note
threshold and attack margin (Sensitivity) on a grid around its value, keeping the grid point of least cost (the
probes and traces it changes). The narrow valleys between the detector's thresholds (a note released or not) are
what the CMA-ES steps over.

    python -I br_polish.py [passes]     updates measurements/wbassrider/detector_fit.json
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import br_fit as F


def items(td, key):
    """the probes and traces whose setting parameters a change of key can move"""
    kind, k = key
    def hit(st):
        resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
        if kind == 'depth':
            return resp == k[0] and abs(sens - k[1]) < 25.0
        return abs(sens - k) < 25.0
    return ([i for i, p in enumerate(F.PROBES) if hit(p['settings'])], [j for j, t in enumerate(F.TRACES) if hit(t['settings'])])


def part_cost(g, td, PI, TI):
    pe = list(F.POOL.map(lambda i: np.minimum(np.abs(F.model_gains(i, g, td) - F.PROBES[i]['ref']), 2.0).sum(), PI))
    te = list(F.POOL.map(lambda j: np.minimum(np.abs(F.trace_gains(j, g, td) - F.TRACES[j]['ref']), 2.0).sum(), TI))
    return float(sum(pe)) + F.TW * float(sum(te))


def grid(kind, v):
    if kind == 'depth': return sorted(set([v] + [v * f for f in (0.5, 0.7, 0.8, 0.9, 0.95, 1.05, 1.1, 1.25, 1.5, 2.0)]))
    return sorted(set([v] + [v + d for d in (-1.0, -0.5, -0.25, -0.1, -0.05, 0.05, 0.1, 0.25, 0.5, 1.0)]))


def main(passes):
    g, td = F.load_fit()
    thr, depth, margin = td
    keys = [('depth', k) for k in F.SETTINGS] + [('thr_u', s) for s in F.SENS] + [('margin', s) for s in F.SENS]
    for n in range(passes):
        moved = 0
        for key in keys:
            kind, k = key
            table = {'depth': depth, 'thr_u': thr, 'margin': margin}[kind]
            PI, TI = items(td, key)
            v0 = table[k]; base = part_cost(g, td, PI, TI); best = (base, v0)
            for v in grid(kind, v0):
                if v == v0: continue
                table[k] = v; c = part_cost(g, td, PI, TI)
                if c < best[0] - 1e-6: best = (c, v)
            table[k] = best[1]
            if best[1] != v0:
                moved += 1
                print('pass %d %-6s %-14s %8.3f -> %8.3f   cost of its items %.2f -> %.2f' % (n, kind, k, v0, best[1], base, best[0]), flush=True)
        total = F.cost(F.pack(g, td))
        F.save(F.pack(g, td), total)
        print('pass %d: %d moved, total cost %.2f' % (n, moved, total), flush=True)
        if not moved: break


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 3)
