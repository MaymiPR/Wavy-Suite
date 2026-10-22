"""WComp Phase 4: refit the current release law (the second form, hcfast2.c with w0 and charging from R: what the
plug-in uses) on probes from a settled state (ident_hc_rel4.py) and the realistic envelopes (ident_hc_real.py),
instead of the start-up probes it was first fitted on. The start-up state itself is not modelled by this form.

    python -I ident_hc_rel8.py fit [nfev]       writes measurements/hcomp/release_law.json and release_w0.json
    python -I ident_hc_rel8.py report
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import optimize
import ident_hc_release2 as I2
import ident_hc_w0 as W
import ident_hc_real as IR
import ident_hc_rel4 as I4


NULLSET = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'hc_nullset_cases.npz')
WEIGHT = 3          # the null-test material counts this many times


def collect_nullset():
    """the null-test signals' envelopes on the alternating probe, at the null test's first six settings"""
    import nulltest_wcomp as N, refhost as R
    S = N.signals(48000)
    out = []
    for st in N.SETTINGS[:6]:
        thr, rat, att, rel, pu = st[:5]
        for name in ('voice-like', 'drum loop', 'bass line', 'white noise -20 rms', 'pink noise -20 rms', 'noise bursts', 'AM 5 Hz 3 kHz', 'sine 100 Hz -20', 'log sweep -20', 'tone steps -50/-10/-30'):
            e = np.abs(S[name].astype(np.float64))
            x = (e * (1 - 2 * (np.arange(len(e)) % 2))).astype(np.float32)
            y = R.render_hc(x, 48000, threshold=thr, ratio=rat, attack=att, release=rel, punch=0.0, output=-18.0)[0]
            out.append(dict(name=name, thr=thr, rat=rat, att=att, rel=rel, a=e, g=y / np.where(e > 1e-6, x, 1)))
        print('setting', st[:4], 'done', flush=True)
    np.savez(NULLSET, cases=np.array(out, dtype=object))


MEMSET = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'hc_memory_cases.npz')


def collect_memory():
    """double bursts from a settled state: a charge (50 or 500 ms), a quiet gap, a 5 ms burst: how long the reference
    remembers the first burst (its release of the second)"""
    import refhost as R
    out = []
    for thr, rat, L in ((-30.0, 4.0, 0.08), (-30.0, 4.0, 0.2), (-30.0, 50.0, 1.0)):
        for rel in (30.0, 100.0, 300.0):
            for first in (0.05, 0.5):
                for G in (0.02, 0.05, 0.1, 0.2, 0.4):
                    e = np.concatenate([np.full(I4.LEAD, I4.QUIET), np.full(int(first * 48000), L), np.full(int(G * 48000), I4.QUIET),
                                        np.full(int(0.005 * 48000), L), np.full(int(0.15 * 48000), I4.QUIET)])
                    x = I4.alt(e)
                    y = R.render_hc(x, 48000, threshold=thr, ratio=rat, attack=0.5, release=rel, punch=0.0, output=-18.0)[0]
                    g = (y / x).astype(np.float64); g /= np.abs(g[I4.LEAD - 4000:I4.LEAD - 3000]).mean()
                    out.append(dict(a=e[I4.SKIP:], g=np.abs(g[I4.SKIP:]), thr=thr, rat=rat, att=0.5, rel=rel, name='memory L %.2f %g ms gap %g ms' % (L, first * 1000, G * 1000)))
        print('depth', thr, rat, L, 'done', flush=True)
    np.savez(MEMSET, cases=np.array(out, dtype=object))


def cases():
    cs = I4.load_cases(with_startup=False)
    if os.path.exists(MEMSET):
        mem = list(np.load(MEMSET, allow_pickle=True)['cases'])
        for c in mem: c['Rt'] = I4.target(c['a'], c['thr'], c['rat']); c['startup'] = False
        cs += mem
    for c in cs: c['ref'] = c['g']; c['from'] = 0
    for c in IR.prepare(list(np.load(IR.CACHE, allow_pickle=True)['cases'])):
        ok = ~np.isnan(c['g'])
        ref = np.abs(c['g'] * c['norm']); ref[~ok] = np.nan
        c['ref'] = ref; c['from'] = int(0.4 * 48000)                  # past the reference's start-up
        c['name'] = 'real ' + c['name']
        cs.append(c)
    if os.path.exists(NULLSET):
        for c in IR.prepare(list(np.load(NULLSET, allow_pickle=True)['cases'])):
            ok = (c['a'] > 1e-4)
            ref = np.abs(c['g'] * c['norm']); ref[~ok] = np.nan
            c['ref'] = ref; c['from'] = int(0.4 * 48000); c['name'] = 'null ' + c['name']
            for _ in range(WEIGHT): cs.append(c)
    return cs


def law(q, x0, nF, nA):
    x = x0.copy(); x[:nF + nA] = q[:nF + nA]
    return W.apply(q[nF + nA:], x)


def residuals(q, cs, x0, nF, nA, dec=16):
    X = law(q, x0, nF, nA)
    r = []
    for c in cs:
        m = IR.run(c, X, c['Rt'])
        s = slice(c['from'], None, dec)
        ref = c['ref'][s]; ok = ~np.isnan(ref)
        r.append(20 * np.log10(np.maximum(m[s][ok], 1e-9)) - 20 * np.log10(np.maximum(ref[ok], 1e-9)))
    return np.concatenate(r)


def setup():
    I2.dll.hc_set_charge_from_r(1)
    x0 = np.array(json.load(open(I2.OUT))['x'])
    z0 = np.array(json.load(open(W.OUT))['z'])
    nF, nA = len(I2.KN['Fs']), len(I2.KN['As'])
    return x0, z0, nF, nA


def fit(nfev):
    x0, z0, nF, nA = setup()
    cs = cases()
    q0 = np.concatenate([x0[:nF + nA], z0])
    r = residuals(q0, cs, x0, nF, nA); print('start rms %.3f dB worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(residuals, q0, args=(cs, x0, nF, nA), diff_step=1e-3, max_nfev=nfev)
    r = residuals(s.x, cs, x0, nF, nA); print('fit rms %.3f dB worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    x = x0.copy(); x[:nF + nA] = s.x[:nF + nA]
    lw = json.load(open(I2.OUT)); lw['x'] = list(map(float, x)); json.dump(lw, open(I2.OUT, 'w'), indent=1)
    json.dump({'S': W.SX.tolist(), 'z': list(map(float, s.x[nF + nA:]))}, open(W.OUT, 'w'))
    report(cs)


def report(cs=None):
    x0, z0, nF, nA = setup()
    cs = cases() if cs is None else cs
    q = np.concatenate([x0[:nF + nA], z0])
    groups = {}
    for c in cs:
        r = residuals(q, [c], x0, nF, nA, dec=4)
        groups.setdefault(c['name'].split()[0], []).append(np.sqrt(np.mean(r ** 2)))
    for k, v in groups.items(): print('  %-8s median rms %.3f dB, mean %.3f (%d)' % (k, np.median(v), np.mean(v), len(v)))


if __name__ == '__main__':
    if sys.argv[1] == 'collect': collect_nullset()
    elif sys.argv[1] == 'collectmem': collect_memory()
    elif sys.argv[1] == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30)
    else: report()
