"""Compares the riding model (vrmodel) with the reference renders (vr_ref): per case the null (output difference
against the reference's output, dB) and the gain error (rms dB where the input is loud enough to read).

    python -I vr_eval.py probes|corpus [setting ...]
    import vr_eval as E; E.cases(kind, settings), E.score(params, cases)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import vr_ref as Q
import vrmodel as M
from measure_voxrider import gtrace, FS


def model_settings(setting):
    """the model's settings for a vr_ref setting (the host's Idle text is a place in the range over a fixed +-12)"""
    comp, hp = Q.SETTINGS[setting]
    s = dict(M.SET_DEFAULT)
    s['target'] = hp.get('target', -21.0); s['rmin'] = hp.get('range_min', -6.0); s['rmax'] = hp.get('range_max', 6.0)
    s['vs'] = hp.get('vocal_sensitivity', 0.0); s['spill'] = hp.get('spill', -48.0)
    s['fast'] = 1.0 if hp.get('attack', 'Slow ').startswith('Fast') else 0.0
    s['live'] = 1.0 if 'Live' in comp else 0.0
    frac = round((hp.get('idle', 0.0) + 12.0) / 24.0, 2)
    s['idle'] = s['rmin'] + frac * (s['rmax'] - s['rmin'])
    return s


def cases(kind, settings=None, keys=None):
    out = []
    for key, _ in Q.items(kind):
        if keys is not None and key not in keys: continue
        for s in (settings or Q.SETTINGS):
            p = Q.path(kind, key, s)
            if not os.path.exists(p): continue
            x, y = Q.load(kind, key, s)
            x = x.astype(np.float64); y = y.astype(np.float64)
            g = gtrace(x, y, win=0.005)
            loud = np.convolve(x * x, np.ones(240) / 240, 'same') > 10 ** (-70 / 10)
            out.append(dict(kind=kind, key=key, setting=s, x=x, y=y, g=g, loud=loud, st=model_settings(s)))
    return out


def evaluate(params, c):
    gm = M.ride(c['x'].astype(np.float32), c['st'], params).astype(np.float64)
    ym = c['x'] * 10 ** (gm / 20)
    e = ym - c['y']
    null = 10 * np.log10((np.sum(e * e) + 1e-20) / (np.sum(c['y'] * c['y']) + 1e-20))
    m = c['loud']
    gerr = np.sqrt(np.mean((gm[m] - c['g'][m]) ** 2)) if m.any() else 0.0
    return null, gerr, gm


def score(params, cs):
    r = [evaluate(params, c)[:2] for c in cs]
    return np.array(r)


if __name__ == '__main__':
    kind = sys.argv[1]; sets = sys.argv[2:] or None
    cs = cases(kind, sets)
    rows = []
    for c in cs:
        n, ge, _ = evaluate({}, c)
        rows.append((c['key'], c['setting'], n, ge))
    for k, s, n, ge in rows: print('%-16s %-10s null %+6.1f dB  gain err %5.2f dB' % (k, s, n, ge))
    print('median null %.1f dB, median gain err %.2f dB over %d cases' % (np.median([r[2] for r in rows]), np.median([r[3] for r in rows]), len(rows)))
