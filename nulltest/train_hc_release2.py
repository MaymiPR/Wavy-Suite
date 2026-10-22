"""WComp Phase 4: train H-Comp's release law (hcgrad.c: the model of train_hc_release.py with its exact gradient in C)
with Adam on the probes of build/hc_torch_set.npz.

    python -I train_hc_release2.py check            gradient against finite differences
    python -I train_hc_release2.py train [iters]    writes measurements/hcomp/release_law6.json
    python -I train_hc_release2.py report
"""
import sys, os, json, time
import numpy as np
import ctypes as ct
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, '..', 'build')
OUT = os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law6.json')
FX = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 25, 30, 40, 50], float)
AX = np.array([0, 0.5, 1, 1.5, 2, 3, 5], float)
WX = np.array([0, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 20, 30], float)
NIN, NH = 5, 8
NNET = NH * NIN + NH + NH + 1
NP = len(FX) + len(AX) + len(WX) + 2 + NNET
DEC = 4
dll = ct.CDLL(os.path.join(HERE, 'hcgrad.dll'))
dll.hc_grad.restype = ct.c_double
dll.hc_set_soft.argtypes = [ct.c_double]
P = lambda v: v.ctypes.data_as(ct.c_void_p)


def load():
    d = np.load(os.path.join(BUILD, 'hc_torch_set.npz'))
    cases = []
    for i in range(len(d['Rt'])):
        m = d['M'][i].astype(np.float64)
        n = int(np.flatnonzero(m)[-1]) + 1 if m.any() else 0
        if n == 0: continue
        cases.append(dict(Rt=np.ascontiguousarray(d['Rt'][i, :n], np.float64), Gdb=np.ascontiguousarray(20 * np.log10(np.maximum(d['G'][i, :n], 1e-6)), np.float64),
                          M=np.ascontiguousarray(m[:n]), att=float(d['meta'][i, 0]), rel=float(d['meta'][i, 1]), startup=int(d['meta'][i, 2]), name=str(d['names'][i])))
    return cases


def initial():
    base = json.load(open(os.path.join(BUILD, 'release_law4.avg.json')))['q']
    rng = np.random.default_rng(0)
    net = np.concatenate([rng.normal(0, 0.5, NH * NIN), rng.normal(0, 0.3, NH), np.zeros(NH), [0.0]])
    return np.concatenate([base, net])


def one(p, c, want_y=False):
    g = np.zeros(NP); cnt = np.zeros(1); y = np.empty(len(c['Rt'])) if want_y else None
    L = dll.hc_grad(len(FX), P(FX), len(AX), P(AX), len(WX), P(WX), P(p), P(c['Rt']), P(c['Gdb']), P(c['M']), len(c['Rt']),
                    ct.c_double(c['rel']), ct.c_double(c['att']), c['startup'], DEC, P(g), P(cnt), P(y) if want_y else None)
    return L, g, cnt[0], y


POOL = ThreadPoolExecutor(8)


def loss_grad(p, cases):
    p = np.ascontiguousarray(p, np.float64)
    res = list(POOL.map(lambda c: one(p, c), cases))
    L = sum(r[0] for r in res); cnt = sum(r[2] for r in res)
    g = sum(r[1] for r in res)
    return L / cnt, g / cnt


def check():
    cases = load()[::40]
    p = initial()
    p[-NNET:] += np.random.default_rng(3).normal(0, 0.1, NNET)      # a non-zero output layer, to test every path
    L, g = loss_grad(p, cases)
    for k in list(range(0, len(FX), 3)) + [len(FX) + 1, len(FX) + len(AX) + 3, len(FX) + len(AX) + len(WX), len(FX) + len(AX) + len(WX) + 1] + list(range(NP - NNET, NP, 7)):
        h = 1e-5
        q = p.copy(); q[k] += h; L1, _ = loss_grad(q, cases)
        q = p.copy(); q[k] -= h; L0, _ = loss_grad(q, cases)
        print('param %3d: analytic %+.6e  numeric %+.6e' % (k, g[k], (L1 - L0) / (2 * h)))


def train(iters, soft=0.05, lr=3e-3):
    cases = load()
    dll.hc_set_soft(soft)
    p = np.array(json.load(open(OUT))['p']) if os.path.exists(OUT) else initial()
    m = np.zeros(NP); v = np.zeros(NP)
    t0 = time.time(); best = (1e9, p.copy())
    for it in range(iters):
        L, g = loss_grad(p, cases)
        if L < best[0]: best = (L, p.copy())
        m = 0.9 * m + 0.1 * g; v = 0.999 * v + 0.001 * g * g
        p = p - lr * (m / (1 - 0.9 ** (it + 1))) / (np.sqrt(v / (1 - 0.999 ** (it + 1))) + 1e-8)
        if it % 10 == 0: print('it %4d loss %.4f (rms %.3f dB) %.0f s' % (it, L, L ** 0.5, time.time() - t0), flush=True)
        if it % 50 == 49: save(best[1])
    save(best[1]); print('best rms %.3f dB (soft gates)' % best[0] ** 0.5)
    dll.hc_set_soft(0.0)
    L, _ = loss_grad(best[1], cases); print('hard gates: rms %.3f dB' % L ** 0.5)
    report(best[1], cases)


def save(p):
    json.dump({'p': list(map(float, p)), 'FX': FX.tolist(), 'AX': AX.tolist(), 'WX': WX.tolist(), 'nin': NIN, 'nh': NH,
               'fast_ms': [30.0, 5.0]}, open(OUT, 'w'))


def report(p=None, cases=None):
    p = np.array(json.load(open(OUT))['p']) if p is None else p
    cases = load() if cases is None else cases
    groups = {}
    for c in cases:
        L, _, cnt, _ = one(np.ascontiguousarray(p), c)
        k = c['name'].split()[0] + (' START' if c['startup'] and not c['name'].startswith('real') else '')
        groups.setdefault(k, []).append((L / max(cnt, 1)) ** 0.5)
    for k, v in groups.items(): print('  %-12s median rms %.3f dB, mean %.3f (%d)' % (k, np.median(v), np.mean(v), len(v)))


if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'check': check()
    elif a == 'train': train(int(sys.argv[2]) if len(sys.argv) > 2 else 300, float(sys.argv[3]) if len(sys.argv) > 3 else 0.05,
                             float(sys.argv[4]) if len(sys.argv) > 4 else 3e-3)
    else: report()
