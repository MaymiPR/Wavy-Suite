import numpy as np, os
from scipy.signal import lfilter
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'ramp.npz'))
c = -24; M = 10**(-c/20); dc = 0.01; s = 48000
def law(x): return 0.99*M/(1 + (M-1)*x)
def truepeak(x):
    v = np.abs(x)
    for a in (0.1607, 0.3569, 0.6308): v = np.maximum(v, np.abs(lfilter([a, 1.0], [1.0, a], x)))
    return v
def causal_min(g, H): return np.array([g[max(0, i-H+1):i+1].min() for i in range(len(g))])
def causal_box(g, L):
    cs = np.cumsum(np.concatenate([[0], g])); return np.array([(cs[i+1]-cs[max(0, i-L+1)])/(i+1-max(0, i-L+1)) for i in range(len(g))])
cases = {'impulse 1 smp +0.5': [0.5], 'block 24 smp +0.3': [0.3]*24, 'step +0.3 for 300 ms': [0.3]*400}
PRE = {}
for nm, ev in cases.items():
    n = 900; x = np.full(n, dc); x[300:300+len(ev)] += ev
    PRE[nm] = law(truepeak(x))/law(truepeak(np.full(n, dc)))
def model(nm, H, b, cc, dom):
    G = PRE[nm]
    if dom == 'lin': return 20*np.log10(causal_box(causal_box(causal_min(G, H), b), cc))
    g = 20*np.log10(G); return causal_box(causal_box(causal_min(g, H), b), cc)
res = []
for dom in ('lin', 'db'):
  for H in range(58, 72):
    for b in range(30, 46, 1):
      for cc in range(16, 34, 1):
        out = {nm: model(nm, H, b, cc, dom) for nm in cases}
        best = None
        for shift in range(-70, -56):
            e = np.concatenate([np.roll(out[nm], shift)[300-120:300+len(ev)+120][:len(ev)+240] - D[f'{nm}|ref'][s-120:s+len(ev)+120] for nm, ev in cases.items() if nm != 'step +0.3 for 300 ms']
                              + [np.roll(out['step +0.3 for 300 ms'], shift)[300-120:300+60] - D['step +0.3 for 300 ms|ref'][s-120:s+60]])
            r = np.sqrt(np.mean(e**2))
            if best is None or r < best[0]: best = (r, shift)
        res.append((best[0], dom, H, b, cc, best[1]))
res.sort()
for r in res[:8]: print('rms %.4f %s hold %d box %d box %d shift %d' % r)
