"""WComp Phase 4: fit the current release law (the second form, hcfast2.c) with CMA-ES, a derivative-free search that
copes with the rough error surface where least squares and gradient steps stall. Data: ident_hc_rel8.py cases()
(settled probes, double-burst memory probes, realistic envelopes, the null-test material weighted x3).

    python -I cma_hc_release.py [generations] [sigma]     writes measurements/hcomp/release_law.json and release_w0.json
                                                          (the previous ones are kept in build/*.before_cma.json)
"""
import sys, os, json, time, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from concurrent.futures import ThreadPoolExecutor
import ident_hc_rel8 as I8
import ident_hc_real as IR

DEC = 32
POOL = ThreadPoolExecutor(8)
MODE = os.environ.get('HC_OBJ', 'null')       # null: the mean null (dB) over the cases, energy-weighted as a null test; db: rms dB error


EXTRA = os.environ.get('HC_FIT_MEMFLOOR') is not None      # one more parameter: ln of the memory's release-time floor (ms)
FITBETA = os.environ.get('HC_FIT_BETA') is not None         # one more: logit of the charge blend (before the floor's)


def objective(q, cs, x0, nF, nA):
    if FITBETA:
        I8.I2.dll.hc_set_charge_blend(__import__('ctypes').c_double(float(1 / (1 + np.exp(-q[-2 if EXTRA else -1])))))
    if EXTRA:
        I8.I2.dll.hc_set_mem_floor(__import__('ctypes').c_double(float(np.exp(q[-1]))))
        q = q[:-1]
    if FITBETA: q = q[:-1]
    X = I8.law(q, x0, nF, nA)                 # sets the C model's w0 / K / p globals for this candidate

    def one(c):
        m = IR.run(c, X, c['Rt'])
        s = slice(c['from'], None, DEC)
        ref = c['ref'][s]; ok = ~np.isnan(ref)
        if MODE == 'null':
            a = c['a'][s][ok]
            num = np.sum(((m[s][ok] - ref[ok]) * a) ** 2); den = np.sum((ref[ok] * a) ** 2) + 1e-30
            return 10 * np.log10(num / den + 1e-12), 1
        d = 20 * np.log10(np.maximum(m[s][ok], 1e-9)) - 20 * np.log10(np.maximum(ref[ok], 1e-9))
        return np.sum(np.minimum(d * d, 100.0)), len(d)     # each sample's error capped at 10 dB (robust)
    r = list(POOL.map(one, cs))
    if MODE == 'null': return sum(a for a, _ in r) / len(r)
    return np.sqrt(sum(a for a, _ in r) / sum(b for _, b in r))


def cma(f, x0, sigma, gens, lam=16, seed=1):
    rng = np.random.default_rng(seed)
    n = len(x0); mu = lam // 2
    w = np.log(mu + 0.5) - np.log(np.arange(1, mu + 1)); w /= w.sum(); mueff = 1 / np.sum(w ** 2)
    cc = (4 + mueff / n) / (n + 4 + 2 * mueff / n); cs_ = (mueff + 2) / (n + mueff + 5)
    c1 = 2 / ((n + 1.3) ** 2 + mueff); cmu = min(1 - c1, 2 * (mueff - 2 + 1 / mueff) / ((n + 2) ** 2 + mueff))
    damps = 1 + 2 * max(0, np.sqrt((mueff - 1) / (n + 1)) - 1) + cs_
    chiN = np.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    m = x0.copy(); C = np.eye(n); pc = np.zeros(n); ps = np.zeros(n); B = np.eye(n); D = np.ones(n)
    best = (f(m), m.copy()); print('start %.4f dB' % best[0], flush=True)
    for g in range(gens):
        z = rng.standard_normal((lam, n)); y = z @ (B * D).T; X = m + sigma * y
        F = np.array([f(x) for x in X]); idx = np.argsort(F)
        if F[idx[0]] < best[0]: best = (F[idx[0]], X[idx[0]].copy())
        yw = w @ y[idx[:mu]]; m = m + sigma * yw
        Cinvh = B @ np.diag(1 / D) @ B.T
        ps = (1 - cs_) * ps + np.sqrt(cs_ * (2 - cs_) * mueff) * (Cinvh @ yw)
        hs = np.linalg.norm(ps) / np.sqrt(1 - (1 - cs_) ** (2 * (g + 1))) < (1.4 + 2 / (n + 1)) * chiN
        pc = (1 - cc) * pc + hs * np.sqrt(cc * (2 - cc) * mueff) * yw
        C = (1 - c1 - cmu) * C + c1 * (np.outer(pc, pc) + (1 - hs) * cc * (2 - cc) * C) + cmu * (y[idx[:mu]].T * w) @ y[idx[:mu]]
        sigma *= np.exp((cs_ / damps) * (np.linalg.norm(ps) / chiN - 1))
        C = (C + C.T) / 2; D2, B = np.linalg.eigh(C); D = np.sqrt(np.maximum(D2, 1e-20))
        print('gen %3d best %.4f  generation best %.4f  median %.4f  sigma %.3f' % (g, best[0], F[idx[0]], np.median(F), sigma), flush=True)
    return best


def main():
    if os.environ.get('HC_ORDER'): I8.I2.dll.hc_set_order(int(os.environ['HC_ORDER']))      # 1: the attack before the release
    gens = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    sigma = float(sys.argv[2]) if len(sys.argv) > 2 else 0.2
    x0, z0, nF, nA = I8.setup()
    if os.environ.get('HC_ORDER'): I8.I2.dll.hc_set_order(int(os.environ['HC_ORDER']))
    if os.environ.get('HC_CHARGE_OUT'): I8.I2.dll.hc_set_charge_from_out(int(os.environ['HC_CHARGE_OUT']))
    if os.environ.get('HC_BETA'): I8.I2.dll.hc_set_charge_blend(__import__('ctypes').c_double(float(os.environ['HC_BETA'])))
    _L = json.load(open(I8.I2.OUT))
    if not EXTRA: I8.I2.dll.hc_set_mem_floor(__import__('ctypes').c_double(_L.get('memory_floor_ms', 0.0)))
    if os.environ.get('HC_RFLOOR'): I8.I2.dll.hc_set_r_floor(int(os.environ['HC_RFLOOR']))
    I8.I2.dll.hc_set_startup(__import__('ctypes').c_double(json.load(open(I8.I2.OUT)).get('startup_ms', 0.0)))
    cs = I8.cases()
    q0 = np.concatenate([x0[:nF + nA], z0])
    if FITBETA: b0 = float(os.environ['HC_FIT_BETA']); q0 = np.concatenate([q0, [np.log(b0 / (1 - b0))]])
    if EXTRA: q0 = np.concatenate([q0, [np.log(float(os.environ['HC_FIT_MEMFLOOR']))]])
    t0 = time.time(); f0 = objective(q0, cs, x0, nF, nA); print('%d cases, one evaluation %.1f s' % (len(cs), time.time() - t0), flush=True)
    best, q = cma(lambda q: objective(q, cs, x0, nF, nA), q0, sigma, gens)
    print('best %.4f dB (start %.4f)' % (best, f0))
    for p in (I8.I2.OUT, I8.W.OUT):
        shutil.copy(p, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', os.path.basename(p).replace('.json', '.before_cma.json')))
    if EXTRA:
        floor = float(np.exp(q[-1])); q = q[:-1]; print('memory release floor %.1f ms' % floor)
    if FITBETA:
        beta = float(1 / (1 + np.exp(-q[-1]))); q = q[:-1]; print('charge blend %.4f' % beta)
    x = x0.copy(); x[:nF + nA] = q[:nF + nA]
    lw = json.load(open(I8.I2.OUT)); lw['x'] = list(map(float, x)); json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    json.dump({'S': I8.W.SX.tolist(), 'z': list(map(float, q[nF + nA:]))}, open(I8.W.OUT, 'w'))
    if os.environ.get('HC_ORDER'):
        lw = json.load(open(I8.I2.OUT)); lw['attack_first'] = int(os.environ['HC_ORDER']); json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    if os.environ.get('HC_CHARGE_OUT'):
        lw = json.load(open(I8.I2.OUT)); lw['charge_from_output'] = int(os.environ['HC_CHARGE_OUT']); json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    if os.environ.get('HC_BETA'):
        lw = json.load(open(I8.I2.OUT)); lw['charge_blend'] = float(os.environ['HC_BETA']); json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    if EXTRA:
        lw = json.load(open(I8.I2.OUT)); lw['memory_floor_ms'] = floor; json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    if FITBETA:
        lw = json.load(open(I8.I2.OUT)); lw['charge_blend'] = beta; json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)
    if os.environ.get('HC_RFLOOR'):
        lw = json.load(open(I8.I2.OUT)); lw['r_floor_at_s'] = int(os.environ['HC_RFLOOR']); json.dump(lw, open(I8.I2.OUT, 'w'), indent=1)


if __name__ == '__main__':
    main()
