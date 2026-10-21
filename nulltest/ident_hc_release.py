"""WComp Phase 4: identify H-Comp's release (fast and slow envelopes) on burst responses.

    python -I ident_hc_release.py collect     render the reference cases (cached in build/hc_release_cases.npz)
    python -I ident_hc_release.py fit [mode]  fit the envelope parameters of hcfast.c

Bursts of an alternating signal over a -60 dBFS carrier, output -18 dB (the limiter stays out).
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import optimize

FS = 48000
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'hc_release_cases.npz')


def collect():
    import measure_hcomp as M
    out = []
    for rat in (50.0, 2.0):
        for att in (0.5, 7.0):
            for rel in (10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0):
                for dur in (0.005, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0):
                    for lvl in (-20.0, -10.0, 0.0):
                        if rat == 2.0 and (att != 0.5 or lvl == -20.0): continue
                        if rel == 3000.0 and dur not in (0.05, 0.5, 2.0): continue
                        tail = min(6.0, max(0.6, rel / 1000 * 2.5))
                        x = M.alt([-60.0, lvl, -60.0], [0.2, dur, tail])
                        a = np.abs(x).astype(np.float64)
                        y = M.hc(x, threshold=-30.0, ratio=rat, attack=att, release=rel, punch=0.0, output=-18.0)[0]
                        g = M.gain_trace(x, y)
                        g /= np.median(g[: int(0.15 * FS)])
                        out.append(dict(rat=rat, att=att, rel=rel, dur=dur, lvl=lvl, a=a, g=g))
                print('ratio %g attack %g release %g done (%d cases)' % (rat, att, rel, len(out)), flush=True)
    np.savez(CACHE, cases=np.array(out, dtype=object))


def load():
    return list(np.load(CACHE, allow_pickle=True)['cases'])


def model(c, q, mode):
    import hcmodel as H
    fast_rate, slow_k, charge_ms, att_scale, fast_exp = q
    p = H.params(thr=-30.0, ratio=c['rat'], att_c=np.exp(-1 / (c['att'] * att_scale * 1e-3 * FS)), rel_c=0.0,
                 fast_rate=fast_rate, slow_rate=slow_k / (c['rel'] * 1e-3 * FS), charge_c=np.exp(-1 / (charge_ms * 1e-3 * FS)),
                 mode=mode, fast_exp=fast_exp)
    m = H.gain(c['a'], p)
    return m / m[0]


def fit(mode=3):
    cs = load()
    sel = [c for c in cs if c['rel'] <= 1000]

    def resid(q):
        r = []
        for c in sel:
            m = model(c, q, mode)
            r.append(20 * np.log10(m[::16]) - 20 * np.log10(c['g'][::16]))
        return np.concatenate(r)
    q0 = [0.008, 14.2, 150.0, 0.99, 0.0]
    best = optimize.least_squares(resid, q0, x_scale=[0.002, 2.0, 50.0, 0.01, 0.5], diff_step=1e-3, max_nfev=300)
    r = resid(best.x)
    print('mode %d: params %s  rms %.3f dB  max %.2f dB' % (mode, np.round(best.x, 5), np.sqrt(np.mean(r ** 2)), np.abs(r).max()))
    worst = []
    for c in sel:
        e = np.abs(20 * np.log10(model(c, best.x, mode)) - 20 * np.log10(c['g'])).max()
        worst.append((e, c['rat'], c['att'], c['rel'], c['dur'], c['lvl']))
    for w in sorted(worst)[-8:]: print('   worst %.2f dB: ratio %g attack %g release %g burst %g level %g' % w)
    return best.x


if __name__ == '__main__':
    if sys.argv[1] == 'collect': collect()
    else: fit(int(sys.argv[2]) if len(sys.argv) > 2 else 3)
