"""Measure the reference's fast peak reading for isolated short patterns (private)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; c = -24; M = 10**(-c/20); A = 0.1
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'kernel_patterns.npz')
def xdet(g_db): g = 10**(g_db/20); return (0.99*M/g - 1)/(M - 1)
rng = np.random.default_rng(7)
pats = []
for r in np.linspace(-1, 1, 17): pats.append([1.0, r])                       # two-sample patterns
for r in np.linspace(-1, 1, 9): pats.append([1.0, 0.0, r])
for r in np.linspace(-1, 1, 9): pats.append([1.0, 0.0, 0.0, r])
for r in np.linspace(-1, 1, 9): pats.append([1.0, r, 1.0])
for r in np.linspace(-1, 1, 9): pats.append([1.0, r, r, 1.0])
for _ in range(150):
    L = rng.integers(2, 8); p = rng.uniform(-1, 1, L); p /= np.abs(p).max(); pats.append(list(p))
res = []
for p in pats:
    x = np.zeros(FS, np.float32); s = FS//2; x[s:s+len(p)] = A*np.array(p)
    y = R.render(x, FS, comp=c)[0]
    # gain at the sample of largest |x| (the look-ahead ramp is complete there)
    k = int(np.argmax(np.abs(p)))
    g = 20*np.log10(abs(y[s+k])/abs(x[s+k]))
    res.append(xdet(g)/A)
np.savez(OUT, pats=np.array([np.pad(p, (0, 8-len(p))) for p in pats]), lens=np.array([len(p) for p in pats]), read=np.array(res))
for p, r in list(zip(pats, res))[:40]: print(np.round(p, 2), round(r, 3))
