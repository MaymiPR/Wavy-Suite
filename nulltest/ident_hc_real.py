"""WComp Phase 4: refit H-Comp's release law on realistic envelopes.

The detector reads |x|, so any material's envelope e = |x| can be carried by an alternating signal e[n] (-1)^n: the
reference's sample-wise gain is then exact (y / x). The envelopes come from the null-test signals (voice-like, drums,
bass, noise, sweeps, steps); settings span threshold, ratio, attack and release.

    python -I ident_hc_real.py collect      render the cases (build/hc_real_cases.npz)
    python -I ident_hc_real.py fit [nfev]   refit Fs, As, w0, K, p, crest (release_law.json, release_w0.json)
    python -I ident_hc_real.py report
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
from scipy.ndimage import maximum_filter1d
from hcmodel import static_gain_db
import ident_hc_release2 as I
import ident_hc_w0 as W

FS = 48000
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'hc_real_cases.npz')


def collect():
    import measure_hcomp as M
    import nulltest_wcomp as N
    S = N.signals(FS, quick=False)
    names = ['voice-like', 'drum loop', 'bass line', 'white noise -20 rms', 'pink noise -20 rms', 'noise bursts', 'log sweep -20',
             'tone steps -50/-10/-30', 'AM 5 Hz 3 kHz']
    rng = np.random.default_rng(3)
    out = []
    for k in range(140):
        name = names[k % len(names)]
        e = np.abs(S[name]).astype(np.float64)[: int(3.0 * FS)]
        thr = float(rng.choice([-12.0, -18.0, -24.0, -30.0, -36.0]))
        rat = float(rng.choice([1.5, 2.0, 3.0, 4.0, 10.0, 50.0]))
        att = float(rng.choice([0.5, 1.0, 3.0, 7.0, 20.0, 50.0]))
        rel = float(rng.choice([10.0, 30.0, 50.0, 100.0, 300.0, 1000.0]))
        x = (e * (1 - 2 * (np.arange(len(e)) % 2))).astype(np.float32)
        y = M.hc(x, threshold=thr, ratio=rat, attack=att, release=rel, punch=0.0, output=-18.0)[0]
        ok = e > 1e-6
        g = np.where(ok, y / np.where(ok, x, 1.0), np.nan)
        out.append(dict(name=name, thr=thr, rat=rat, att=att, rel=rel, a=e, g=g))
        if k % 10 == 0: print(k, name, thr, rat, att, rel, flush=True)
    np.savez(CACHE, cases=np.array(out, dtype=object))


def target(c):
    a = c['a']; H, lead = 64, 1
    pad = np.concatenate([a, np.full(H + lead, a[-1])])
    h = maximum_filter1d(pad, size=H, origin=-(H // 2))[lead:lead + len(a)]
    return np.ascontiguousarray(-static_gain_db(h, c['thr'], c['rat']), np.float64)


def run(c, x, Rt):
    n = len(Rt); g = np.empty(n)
    xs = np.concatenate([np.asarray(I.KN[k], float) for k in I.ORDER]); ns = np.array([len(I.KN[k]) for k in I.ORDER], np.int32)
    att_c = np.exp(-1 / (c['att'] * 0.99e-3 * FS))
    I.dll.hc_release(Rt.ctypes.data_as(ct.c_void_p), g.ctypes.data_as(ct.c_void_p), None, None, n, ct.c_double(FS), ct.c_double(c['rel']),
                     ct.c_double(att_c), 38, 27, ns.ctypes.data_as(ct.c_void_p), xs.ctypes.data_as(ct.c_void_p),
                     np.ascontiguousarray(x, np.float64).ctypes.data_as(ct.c_void_p))
    return g


def makeup(c):
    # the reference's gain includes makeup and the output (-18): normalise by its gain where nothing compresses
    return None


def residuals(z, x0, cases, Rts, dec=8):
    x = W.apply(z[len(x0):], x0.copy()) if False else None
    r = []
    for c, Rt in zip(cases, Rts):
        m = run(c, X_CUR, Rt)
        ref = c['g'] * c['norm']
        ok = ~np.isnan(ref)
        d = 20 * np.log10(np.maximum(m[ok][::dec], 1e-9)) - 20 * np.log10(np.maximum(np.abs(ref[ok][::dec]), 1e-9))
        r.append(d)
    return np.concatenate(r)


X_CUR = None


_MK = np.loadtxt(os.path.join(os.path.dirname(I.OUT), 'makeup.csv'), delimiter=',', skiprows=1)
_MR = np.loadtxt(os.path.join(os.path.dirname(I.OUT), 'makeup_release.csv'), delimiter=',', skiprows=1)


def makeup_db(thr, rat, rel):
    """The reference's makeup (measured grid at Release 100, scaled by the release factor)."""
    rats = sorted(set(_MK[:, 1])); ks = []
    for r in rats:
        s_ = _MK[(_MK[:, 1] == r) & (_MK[:, 0] <= -12)]
        ks.append(max(0.0, -np.polyfit(s_[:, 0], s_[:, 2], 1)[0]))
    k = np.interp(1 / rat, [1 / r for r in rats[::-1]], ks[::-1])
    sel = _MR[(_MR[:, 0] == -30.0) & (_MR[:, 1] == 4.0)]
    base = sel[sel[:, 2] == 100.0][0, 3]
    f = np.interp(np.log(rel), np.log(sel[:, 2]), sel[:, 3] / base)
    return k * max(0.0, -9.0 - thr) * f


def prepare(cases):
    # the reference's gain without makeup and the output (-18 dB)
    for c in cases:
        c['Rt'] = target(c)
        c['norm'] = 10 ** (-(makeup_db(c['thr'], c['rat'], c['rel']) - 18.0) / 20)
    return cases


def fit(nfev):
    global X_CUR
    cs = prepare(list(np.load(CACHE, allow_pickle=True)['cases']))
    x0 = np.array(json.load(open(I.OUT))['x'])
    z0 = np.array(json.load(open(W.OUT))['z'])
    I.dll.hc_set_charge_from_r(1)
    nF, nA = len(I.KN['Fs']), len(I.KN['As'])
    q0 = np.concatenate([x0[:nF + nA], z0])
    Rts = [c['Rt'] for c in cs]

    def f(q):
        global X_CUR
        x = x0.copy(); x[:nF + nA] = q[:nF + nA]
        X_CUR = W.apply(q[nF + nA:], x)
        return residuals(None, x0, cs, Rts)
    r = f(q0); print('start: rms %.3f dB, worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(f, q0, diff_step=1e-3, max_nfev=nfev, loss='soft_l1', f_scale=0.5)
    r = f(s.x); print('fit: rms %.3f dB, worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    x = x0.copy(); x[:nF + nA] = s.x[:nF + nA]
    law = json.load(open(I.OUT)); law['x'] = list(map(float, x)); json.dump(law, open(I.OUT, 'w'), indent=1)
    json.dump({'S': W.SX.tolist(), 'z': list(map(float, s.x[nF + nA:]))}, open(W.OUT, 'w'))
    report(cs)


def report(cs=None):
    global X_CUR
    if cs is None: cs = prepare(list(np.load(CACHE, allow_pickle=True)['cases']))
    x0 = np.array(json.load(open(I.OUT))['x']); z0 = np.array(json.load(open(W.OUT))['z'])
    I.dll.hc_set_charge_from_r(1)
    X_CUR = W.apply(z0, x0)
    rows = []
    for c in cs:
        r = residuals(None, x0, [c], [c['Rt']], dec=1)
        rows.append((np.sqrt(np.mean(r ** 2)), np.abs(r).max(), c['name'], c['thr'], c['rat'], c['att'], c['rel']))
    rows.sort()
    print('median rms %.3f dB, mean rms %.3f' % (np.median([r[0] for r in rows]), np.mean([r[0] for r in rows])))
    for r in rows[-10:]: print('   rms %.3f worst %.2f dB: %s thr %g ratio %g attack %g release %g' % r)


if __name__ == '__main__':
    if sys.argv[1] == 'collect': collect()
    elif sys.argv[1] == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30)
    else: report()
