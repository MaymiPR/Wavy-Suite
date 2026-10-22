"""WComp Phase 4: H-Comp's release, fourth form (hcfast4.c), on probes that start from a settled state.

H-Comp releases slowly for the first 0.1-0.3 s after a reset (a hidden start-up state: no gain reduction, but a short
event that would release at once releases at the Release time instead). Probes with a short quiet lead-in therefore
measure the start-up, not the steady behaviour; these have a 1 s quiet lead-in (the start-up probes of ident_hc_rel3.py
are kept for the start-up state itself).

The probe is the alternating signal e[n] (-1)^n (the detector reads |x|, the gain is y / x exactly).

    python -I ident_hc_rel4.py collect          build/hc_rel4_cases.npz
    python -I ident_hc_rel4.py fit [nfev]       measurements/hcomp/release_law4.json
    python -I ident_hc_rel4.py report
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
CACHE = os.path.join(BUILD, 'hc_rel4_cases.npz')
OUT = os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law4.json')
LEAD, QUIET = FS, 0.0003
AVG = 1                                 # S is the plain average of R (hcfast4.c avg)
FORM = 5                                # 5: a second, faster average S2 with its own table (hcfast5.c)
SKIP = LEAD - 2400                      # the simulation starts 50 ms before the event (the state settled at rest)

DEPTHS = [(-30.0, 4.0, 0.054), (-30.0, 4.0, 0.08), (-30.0, 4.0, 0.2), (-30.0, 50.0, 0.5), (-30.0, 50.0, 1.0), (-40.0, 2.0, 1.0)]
DURS = [0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0]
RELEASES = [30.0, 100.0, 300.0, 1000.0]


def alt(e):
    return (e * (1 - 2 * (np.arange(len(e)) % 2))).astype(np.float32)


def cases_spec():
    rng = np.random.default_rng(7)
    spec = []
    for thr, rat, L in DEPTHS:
        for D in DURS:
            for rel in RELEASES:
                e = np.concatenate([np.full(int(D * FS), L), np.full(int((0.1 + 1.5 * rel / 1000) * FS), QUIET)])
                spec.append(('burst L %.3f %g ms' % (L, D * 1000), e, thr, rat, 0.5, rel))
    # the attack: the reduction not yet reached when the event ends
    for att in (7.0, 30.0):
        for D in (0.004, 0.02, 0.1, 0.5):
            for rel in (100.0, 300.0):
                e = np.concatenate([np.full(int(D * FS), 0.08), np.full(int(0.4 * FS), QUIET)])
                spec.append(('burst att %g %g ms' % (att, D * 1000), e, -30.0, 4.0, att, rel))
    # pulse trains (crest factors): width w every T, for 0.6 s
    for w, T in ((0.001, 0.007), (0.002, 0.007), (0.004, 0.01), (0.002, 0.02), (0.005, 0.05), (0.01, 0.03), (0.02, 0.04)):
        for L, thr, rat in ((0.2, -30.0, 4.0), (1.0, -30.0, 50.0)):
            for rel in (100.0, 300.0):
                per = np.full(int(T * FS), QUIET); per[:int(w * FS)] = L
                e = np.concatenate([np.tile(per, int(0.6 / T)), np.full(int(0.3 * FS), QUIET)])
                spec.append(('pulses %g/%g ms L %.1f' % (w * 1000, T * 1000, L), e, thr, rat, 0.5, rel))
    # steps down to a level still above the threshold, after short and long holds
    for D in (0.02, 0.2, 1.0):
        for L2 in (0.1, 0.05):
            for rel in (100.0, 300.0):
                e = np.concatenate([np.full(int(D * FS), 1.0), np.full(int(0.5 * FS), L2), np.full(int(0.2 * FS), QUIET)])
                spec.append(('step 1.0 %g ms -> %.2f' % (D * 1000, L2), e, -40.0, 4.0, 0.5, rel))
    # random envelopes with a settled start: syllables and hits (smooth and sharp), random levels
    for k in range(40):
        n = int(1.5 * FS); t = np.arange(n) / FS
        if k % 2 == 0:
            f = rng.uniform(2, 6); e = 10 ** (rng.uniform(-24, 0) / 20) * np.clip(np.sin(2 * np.pi * f * t + rng.uniform(0, 6)), 0, None) ** 2
            pitch = rng.uniform(80, 250); e = e * np.abs(sum(np.sin(2 * np.pi * pitch * j * t) / j for j in range(1, 10))) / 2.0
        else:
            e = np.zeros(n)
            for h in rng.uniform(0, 1.4, int(rng.integers(3, 12))):
                s0 = int(h * FS); m = min(n - s0, int(0.3 * FS)); tt = np.arange(m) / FS
                e[s0:s0 + m] += 10 ** (rng.uniform(-20, 0) / 20) * np.exp(-tt / rng.uniform(0.01, 0.15)) * np.abs(rng.standard_normal(m) * 0.5 + np.sin(2 * np.pi * 60 * tt))
        e = np.maximum(e, QUIET)
        spec.append(('random %d' % k, e, float(rng.choice([-12.0, -18.0, -24.0, -30.0, -36.0])), float(rng.choice([2.0, 3.0, 4.0, 10.0, 50.0])),
                     float(rng.choice([0.5, 3.0, 7.0, 20.0])), float(rng.choice([30.0, 100.0, 300.0, 1000.0]))))
    return spec


def collect():
    import refhost as R
    out = []
    for i, (name, body, thr, rat, att, rel) in enumerate(cases_spec()):
        e = np.concatenate([np.full(LEAD, QUIET), body])
        x = alt(e)
        y = R.render_hc(x, FS, threshold=thr, ratio=rat, attack=att, release=rel, punch=0.0, output=-18.0)[0]
        g = (y / x).astype(np.float64)
        g /= np.abs(g[LEAD - 4000:LEAD - 3000]).mean()          # the uncompressed gain (makeup and output)
        out.append(dict(a=e[SKIP:], g=np.abs(g[SKIP:]), thr=thr, rat=rat, att=att, rel=rel, name=name))
        if i % 25 == 0: print(i, name, flush=True)
    np.savez(CACHE, cases=np.array(out, dtype=object))
    print('saved', len(out))


def target(a, thr, rat):
    H, lead = 64, 1
    pad = np.concatenate([a, np.full(H + lead, a[-1])])
    h = maximum_filter1d(pad, size=H, origin=-(H // 2))[lead:lead + len(a)]
    return np.ascontiguousarray(-static_gain_db(h, thr, rat), np.float64)


# ---- model (hcfast4.c) ----
FX = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 25, 30, 40, 50], float)
AX = np.array([0, 0.5, 1, 1.5, 2, 3, 5], float)
WX = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 20, 30], float)       # R - S (dB)
dll = None


def load_dll():
    global dll
    if dll is None: dll = ct.CDLL(os.path.join(HERE, 'hcfast%d.dll' % FORM))
    return dll


def unpack(q):
    i = 0
    Fv = q[i:i + len(FX)]; i += len(FX)
    Av = q[i:i + len(AX)]; i += len(AX)
    Wv = q[i:i + len(WX)]; i += len(WX)
    crest, S0 = np.exp(q[i]), q[i + 1]; i += 2
    if FORM == 5:
        W2v = q[i:i + len(WX)]; i += len(WX)
        return Fv, Av, Wv, crest, S0, W2v, np.exp(q[i]), q[i + 1]
    return Fv, Av, Wv, crest, S0


def run(q, Rt, att, rel, S_init=0.0, startup=False):
    d = load_dll()
    u = unpack(q)
    Fv, Av, Wv, crest, S0 = u[:5]
    Fv, Av, Wv = [np.ascontiguousarray(v, np.float64) for v in (Fv, Av, Wv)]
    W2v = np.ascontiguousarray(u[5], np.float64) if FORM == 5 else None   # kept alive for the call
    n = len(Rt); g = np.empty(n)
    p = lambda v: v.ctypes.data_as(ct.c_void_p)
    att_c = np.exp(-1 / (att * 0.99e-3 * FS))
    getattr(d, "hc%d" % FORM)(p(Rt), p(g), None, None, n, ct.c_double(FS), ct.c_double(rel), ct.c_double(att_c), 38, 27,
          len(FX), p(FX), p(Fv), len(AX), p(AX), p(Av), len(WX), p(WX), p(Wv), ct.c_double(crest), ct.c_double(S0 if startup else S_init), AVG,
          *(() if FORM != 5 else (len(WX), p(WX), p(W2v), ct.c_double(u[6]), ct.c_double(u[7] if startup else 0.0))))
    return g


def initial():
    law = json.load(open(os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law.json')))
    kn, x = law['knots'], np.array(law['x'])
    i = 0; parts = {}
    for k in law['order']: parts[k] = x[i:i + len(kn[k])]; i += len(kn[k])
    Fv = np.interp(FX, kn['Fs'], parts['Fs']); Av = np.interp(AX, kn['As'], parts['As'])
    # the release time shrinks as R moves above the slow state: about 1 at 0 dB, 1/100 by 6 dB
    Wv = np.interp(WX, [0, 1, 3, 6, 12, 30], np.log([1.0, 0.5, 0.1, 0.01, 0.002, 0.001]))
    return np.concatenate([Fv, Av, Wv, [np.log(150.0), 30.0]])


def load_cases(with_startup=True):
    cs = list(np.load(CACHE, allow_pickle=True)['cases'])
    for c in cs: c['Rt'] = target(c['a'], c['thr'], c['rat']); c['startup'] = False
    if with_startup:
        import ident_hc_rel3 as I3
        st = I3.load_cases()[::4]                    # every fourth start-up probe (20 ms lead-in, a fresh plug-in)
        for c in st: c['startup'] = True
        cs += st
    return cs


def residuals(q, cs, dec=16):
    r = []
    for c in cs:
        m = run(q, c['Rt'], c['att'], c['rel'], startup=c['startup'])
        s = slice(0 if not c['startup'] else 800, None, dec)
        r.append(20 * np.log10(np.maximum(m[s], 1e-9)) - 20 * np.log10(np.maximum(c['g'][s], 1e-9)))
    return np.concatenate(r)


def fit(nfev):
    cs = load_cases()
    q0 = np.array(json.load(open(OUT))['q']) if os.path.exists(OUT) else initial()
    if FORM == 5 and len(q0) == len(FX) + len(AX) + len(WX) + 2:
        # start the second average with no effect (w2 = 1) and a 30 ms time constant
        q0 = np.concatenate([q0, np.zeros(len(WX)), [np.log(30.0), 5.0]])
    r = residuals(q0, cs); print('start rms %.3f dB worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(residuals, q0, args=(cs,), diff_step=1e-3, max_nfev=nfev)
    r = residuals(s.x, cs); print('fit rms %.3f dB worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'q': list(map(float, s.x)), 'FX': FX.tolist(), 'AX': AX.tolist(), 'WX': WX.tolist(), 'avg': AVG, 'form': FORM}, open(OUT, 'w'))
    report(s.x)


def report(q=None):
    q = np.array(json.load(open(OUT))['q']) if q is None else q
    u = unpack(q); Fv, Av, Wv, crest, S0 = u[:5]
    print('crest %.1f ms, start-up S0 %.1f dB, w(R - S):' % (crest, S0), ' '.join('%g:%.3g' % (a, np.exp(b)) for a, b in zip(WX, Wv)))
    if FORM == 5: print('fast average %.1f ms, start-up %.1f dB, w2(R - S2):' % (u[6], u[7]), ' '.join('%g:%.3g' % (a, np.exp(b)) for a, b in zip(WX, u[5])))
    cs = load_cases()
    rows = []
    for c in cs:
        r = residuals(q, [c], dec=4)
        rows.append((np.sqrt(np.mean(r ** 2)), np.abs(r).max(), ('START ' if c['startup'] else '') + c['name'], c['thr'], c['rat'], c['att'], c['rel']))
    rows.sort()
    print('probe cases: median rms %.3f dB, mean %.3f' % (np.median([r[0] for r in rows]), np.mean([r[0] for r in rows])))
    for r in rows[-10:]: print('   rms %.3f worst %.2f: %s thr %g ratio %g attack %g release %g' % r)


if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'collect': collect()
    elif a == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30)
    else: report()
