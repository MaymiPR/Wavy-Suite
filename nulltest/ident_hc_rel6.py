"""WComp Phase 4: H-Comp's release, sixth form (hcfast6.c): the fourth form (ident_hc_rel4.py, slow average) with a
learned correction of the release-time factor (a small MLP over R and its distance to three averages).

    python -I ident_hc_rel6.py fit [nfev] [joint]    measurements/hcomp/release_law6.json (joint: the tables too)
    python -I ident_hc_rel6.py report
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import ctypes as ct
from scipy import optimize
import ident_hc_rel4 as I4
I4.FORM = 4                                         # the base law is the fourth form (one average)

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law6.json')
NIN, NH = 5, 8
NNET = NH * NIN + NH + NH + 1
dll = ct.CDLL(os.path.join(HERE, 'hcfast6.dll'))
BASE = os.path.join(HERE, '..', 'build', 'release_law4.avg.json')      # the fitted fourth form (averaging S)


def init_net(seed=0):
    rng = np.random.default_rng(seed)
    w1 = rng.normal(0, 0.5, (NH, NIN)); b1 = rng.normal(0, 0.3, NH)
    return np.concatenate([w1.ravel(), b1, np.zeros(NH), [0.0]])      # output weights 0: no correction at the start


def run(base, net, Rt, att, rel, startup=False):
    Fv, Av, Wv, crest, S0 = I4.unpack(base)[:5]
    Fv, Av, Wv, net = [np.ascontiguousarray(v, np.float64) for v in (Fv, Av, Wv, net)]
    n = len(Rt); g = np.empty(n)
    p = lambda v: v.ctypes.data_as(ct.c_void_p)
    att_c = np.exp(-1 / (att * 0.99e-3 * FS))
    dll.hc6(p(Rt), p(g), None, n, ct.c_double(FS), ct.c_double(rel), ct.c_double(att_c), 38, 27,
            len(I4.FX), p(I4.FX), p(Fv), len(I4.AX), p(I4.AX), p(Av), len(I4.WX), p(I4.WX), p(Wv), ct.c_double(crest),
            ct.c_double(S0 if startup else 0.0), p(net))
    return g


def residuals(base, net, cs, dec=16):
    r = []
    for c in cs:
        m = run(base, net, c['Rt'], c['att'], c['rel'], startup=c['startup'])
        s = slice(0 if not c['startup'] else 800, None, dec)
        r.append(20 * np.log10(np.maximum(m[s], 1e-9)) - 20 * np.log10(np.maximum(c['g'][s], 1e-9)))
    return np.concatenate(r)


def load_all():
    cs = I4.load_cases()
    # the realistic envelopes of ident_hc_real.py too (fresh plug-in: start-up state), gain normalised by the makeup
    import ident_hc_real as IR
    for c in IR.prepare(list(np.load(IR.CACHE, allow_pickle=True)['cases']))[::2]:
        ok = ~np.isnan(c['g'])
        g = np.abs(c['g'] * c['norm']); g[~ok] = 1.0
        cs.append(dict(a=c['a'], g=g, Rt=c['Rt'], thr=c['thr'], rat=c['rat'], att=c['att'], rel=c['rel'], name='real ' + c['name'], startup=True))
    return cs


def fit(nfev, joint):
    base = np.array(json.load(open(BASE))['q'])
    cs = load_all()
    if os.path.exists(OUT):
        d = json.load(open(OUT)); net = np.array(d['net']); base = np.array(d['base'])
    else:
        net = init_net()
    nb = len(base)
    z0 = np.concatenate([net, base]) if joint else net
    f = (lambda z: residuals(z[NNET:], z[:NNET], cs)) if joint else (lambda z: residuals(base, z, cs))
    r = f(z0); print('start rms %.3f dB worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(f, z0, diff_step=1e-3, max_nfev=nfev, loss='soft_l1', f_scale=1.0)
    z = s.x
    net, base = (z[:NNET], z[NNET:]) if joint else (z, base)
    r = residuals(base, net, cs); print('fit rms %.3f dB worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'net': list(map(float, net)), 'base': list(map(float, base)), 'FX': I4.FX.tolist(), 'AX': I4.AX.tolist(),
               'WX': I4.WX.tolist(), 'nin': NIN, 'nh': NH}, open(OUT, 'w'))
    report(base, net, cs)


def report(base=None, net=None, cs=None):
    if base is None:
        d = json.load(open(OUT)); net = np.array(d['net']); base = np.array(d['base'])
    cs = load_all() if cs is None else cs
    groups = {}
    for c in cs:
        r = residuals(base, net, [c], dec=4)
        k = c['name'].split()[0] + (' START' if c['startup'] and not c['name'].startswith('real') else '')
        groups.setdefault(k, []).append(np.sqrt(np.mean(r ** 2)))
    for k, v in groups.items(): print('  %-12s median rms %.3f dB, mean %.3f (%d)' % (k, np.median(v), np.mean(v), len(v)))


if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30, 'joint' in sys.argv)
    else: report()
