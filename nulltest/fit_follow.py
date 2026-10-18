import numpy as np, os
from scipy.signal import lfilter
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'diag_noise.npz'))
x, ref = D['x'].astype(float), D['ref'].astype(float)
FS = 48000; c = -24; M = 10**(-c/20); n = len(x)
m = np.abs(x) > 1e-3
g = np.full(n, np.nan); g[m] = np.abs(ref[m]/x[m])
Lr = 20*np.log10(np.maximum((0.99*M/g - 1)/(M - 1), 1e-6))
v = np.abs(x)
for a in (0.1607, 0.3569, 0.6308): v = np.maximum(v, np.abs(lfilter([a, 1.0], [1.0, a], x)))
vd = 20*np.log10(np.maximum(v, 1e-9))
s = slice(FS, None)
def follower_db(rate_db_s, hold):
    out = np.empty(n); st = -200.0; h = 0; dec = rate_db_s/FS
    for i in range(n):
        if vd[i] >= st: st = vd[i]; h = hold
        elif h > 0: h -= 1
        else: st = max(vd[i], st - dec)
        out[i] = st
    return out
def follower_lin(tau_ms):
    k = np.exp(-1/(tau_ms*1e-3*FS)); out = np.empty(n); st = 0.0
    for i in range(n):
        st = v[i] if v[i] >= st else st*k + v[i]*(1-k) if False else max(v[i], st*k)
        out[i] = st
    return 20*np.log10(np.maximum(out, 1e-9))
res = []
for rate in (5, 10, 20, 40, 80):
    for hold in (0, 480, 2400):
        f = follower_db(rate, hold); d = (Lr - f)[s]; d = d[np.isfinite(d)]
        res.append((np.sqrt(np.mean((d - d.mean())**2)), d.mean(), 'dB-decay %g dB/s hold %d' % (rate, hold)))
for tau in (20, 50, 100, 200, 400):
    f = follower_lin(tau); d = (Lr - f)[s]; d = d[np.isfinite(d)]
    res.append((np.sqrt(np.mean((d - d.mean())**2)), d.mean(), 'lin peak-decay tau %g ms' % tau))
res.sort()
for r in res[:10]: print('spread %.3f dB  offset %+.2f  %s' % r)
