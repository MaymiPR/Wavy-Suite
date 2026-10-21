"""WComp Phase 4: H-Comp's release, third form (hcfast3.c): the slow law plus a fast part Ff(R, S).

The probe is the alternating signal e[n] (-1)^n (the detector reads |x|, the gain is y / x exactly): a quiet lead-in,
a charge at a level for a time, then a quiet gap in which the gain reduction releases. The grid covers the depth
(threshold, ratio, level), the charge time and the release time.

    python -I ident_hc_rel3.py collect          build/hc_rel3_cases.npz
    python -I ident_hc_rel3.py fit [nfev]       measurements/hcomp/release_law3.json
    python -I ident_hc_rel3.py report           the probe cases and the realistic envelopes (ident_hc_real.py)
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
from scipy.ndimage import maximum_filter1d
from hcmodel import static_gain_db

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, '..', 'build')
CACHE = os.path.join(BUILD, 'hc_rel3_cases.npz')
OUT = os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law3.json')
LEAD, QUIET = int(0.02 * FS), 0.0003

FX = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 25, 30, 40, 50], float)
AX = np.array([0, 0.5, 1, 1.5, 2, 3, 5], float)
RK = np.array([0, 2, 4, 6, 8, 10, 12, 14, 16, 20, 25, 30, 40, 50], float)
SK = np.array([0, 2, 5, 10, 15, 20, 30, 40], float)
DEPTHS = [(-30.0, 50.0, 1.0), (-40.0, 50.0, 1.0), (-30.0, 50.0, 0.5), (-30.0, 4.0, 0.2), (-30.0, 4.0, 0.08), (-30.0, 4.0, 0.054),
          (-30.0, 4.0, 0.04), (-20.0, 10.0, 1.0), (-40.0, 2.0, 1.0), (-24.0, 3.0, 0.3)]
CHARGES = [0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0]
RELEASES = [10.0, 30.0, 100.0, 300.0, 1000.0]


def probe_env(L, charge, gap):
    return np.concatenate([np.full(LEAD, QUIET), np.full(int(charge * FS), L), np.full(int(gap * FS), QUIET)])


def collect():
    import refhost as R
    out = []
    for thr, rat, L in DEPTHS:
        for ch in CHARGES:
            for rel in RELEASES:
                e = probe_env(L, ch, 0.4 + 1.2 * rel / 1000.0)
                x = (e * (1 - 2 * (np.arange(len(e)) % 2))).astype(np.float32)
                y = R.render_hc(x, FS, threshold=thr, ratio=rat, attack=0.5, release=rel, punch=0.0, output=-18.0)[0]
                g = (y / x).astype(np.float64)
                g /= np.abs(g[LEAD - 200:LEAD - 100]).mean()         # the uncompressed gain (makeup and output)
                out.append(dict(a=e, g=np.abs(g), thr=thr, rat=rat, att=0.5, rel=rel, name='L %.3f charge %g' % (L, ch)))
        print('depth', thr, rat, L, 'done', flush=True)
    np.savez(CACHE, cases=np.array(out, dtype=object))


def target(a, thr, rat):
    H, lead = 64, 1
    pad = np.concatenate([a, np.full(H + lead, a[-1])])
    h = maximum_filter1d(pad, size=H, origin=-(H // 2))[lead:lead + len(a)]
    return np.ascontiguousarray(-static_gain_db(h, thr, rat), np.float64)


dll = None


def load_dll():
    global dll
    if dll is None: dll = ct.CDLL(os.path.join(HERE, 'hcfast3.dll'))
    return dll


def unpack(q):
    i = 0
    Fv = q[i:i + len(FX)]; i += len(FX)
    Av = q[i:i + len(AX)]; i += len(AX)
    crest = np.exp(q[i]); i += 1
    Ff = q[i:i + len(RK) * len(SK)]
    return Fv, Av, crest, Ff


def run(q, Rt, att, rel):
    d = load_dll()
    Fv, Av, crest, Ff = [np.ascontiguousarray(v, np.float64) if isinstance(v, np.ndarray) else v for v in unpack(q)]
    n = len(Rt); g = np.empty(n)
    p = lambda v: v.ctypes.data_as(ct.c_void_p)
    att_c = np.exp(-1 / (att * 0.99e-3 * FS))
    d.hc3(p(Rt), p(g), None, None, n, ct.c_double(FS), ct.c_double(rel), ct.c_double(att_c), 38, 27,
          len(FX), p(FX), p(Fv), len(AX), p(AX), p(Av), ct.c_double(crest), len(RK), p(RK), len(SK), p(SK), p(Ff))
    return g


def initial():
    law = json.load(open(os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law.json')))
    kn, x = law['knots'], np.array(law['x'])
    i = 0; parts = {}
    for k in law['order']: parts[k] = x[i:i + len(kn[k])]; i += len(kn[k])
    Fv = np.interp(FX, kn['Fs'], parts['Fs']); Av = np.interp(AX, kn['As'], parts['As'])
    # the fast part: about 5000 dB/s near 28 dB, 700 near 14, 100 near 11, little below (S = 0); it vanishes as S nears R
    base = np.interp(RK, [0, 8, 11, 14, 20, 28, 50], np.log([1e-2, 5, 100, 700, 2500, 5000, 8000]))
    Ff = np.array([[b - 0.35 * s for s in SK] for b in base]).ravel()
    return np.concatenate([Fv, Av, [np.log(142.0)], Ff])


def load_cases():
    cs = list(np.load(CACHE, allow_pickle=True)['cases'])
    for c in cs: c['Rt'] = target(c['a'], c['thr'], c['rat'])
    return cs


def residuals(q, cs, dec=16):
    r = []
    for c in cs:
        m = run(q, c['Rt'], c['att'], c['rel'])
        s = slice(LEAD - 100, None, dec)
        r.append(20 * np.log10(np.maximum(m[s], 1e-9)) - 20 * np.log10(np.maximum(c['g'][s], 1e-9)))
    return np.concatenate(r)


def fit(nfev):
    cs = load_cases()
    q0 = np.array(json.load(open(OUT))['q']) if os.path.exists(OUT) else initial()
    r = residuals(q0, cs); print('start rms %.3f dB worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(residuals, q0, args=(cs,), diff_step=1e-3, max_nfev=nfev)
    r = residuals(s.x, cs); print('fit rms %.3f dB worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'q': list(map(float, s.x)), 'FX': FX.tolist(), 'AX': AX.tolist(), 'RK': RK.tolist(), 'SK': SK.tolist()}, open(OUT, 'w'))
    report(s.x)


def report(q=None):
    q = np.array(json.load(open(OUT))['q']) if q is None else q
    cs = load_cases()
    rows = []
    for c in cs:
        r = residuals(q, [c], dec=4)
        rows.append((np.sqrt(np.mean(r ** 2)), np.abs(r).max(), c['name'], c['thr'], c['rat'], c['rel']))
    rows.sort()
    print('probe cases: median rms %.3f dB, mean %.3f' % (np.median([r[0] for r in rows]), np.mean([r[0] for r in rows])))
    for r in rows[-8:]: print('   rms %.3f worst %.2f: %s thr %g ratio %g release %g' % r)
    # the realistic envelopes of ident_hc_real.py (their gain normalised by the measured makeup)
    import ident_hc_real as IR
    rc = IR.prepare(list(np.load(IR.CACHE, allow_pickle=True)['cases']))
    rows = []
    for c in rc:
        m = run(q, c['Rt'], c['att'], c['rel'])
        ref = c['g'] * c['norm']; ok = ~np.isnan(ref)
        d = 20 * np.log10(np.maximum(m[ok], 1e-9)) - 20 * np.log10(np.maximum(np.abs(ref[ok]), 1e-9))
        rows.append((np.sqrt(np.mean(d ** 2)), np.abs(d).max(), c['name'], c['thr'], c['rat'], c['att'], c['rel']))
    rows.sort()
    print('realistic envelopes: median rms %.3f dB, mean %.3f' % (np.median([r[0] for r in rows]), np.mean([r[0] for r in rows])))
    for r in rows[-6:]: print('   rms %.3f worst %.2f: %s thr %g ratio %g attack %g release %g' % r)


if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'collect': collect()
    elif a == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30)
    else: report()
