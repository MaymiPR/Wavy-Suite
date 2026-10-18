import numpy as np, os
from scipy.ndimage import minimum_filter1d
D = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'ramp.npz'))
c = -24; M = 10**(-c/20); dc = 0.01
def law_db(x): return 20*np.log10(0.99*M/(1+(M-1)*x))
s = 48000
cases = {'impulse 1 smp +0.5': [0.5], 'block 24 smp +0.3': [0.3]*24}
def kernel(kind, W2):
    u = np.arange(-W2, W2+1, dtype=float)
    if kind == 'box': k = np.ones_like(u)
    elif kind == 'tri': k = W2 + 1 - np.abs(u)
    elif kind == 'hann': k = 0.5*(1 + np.cos(np.pi*u/(W2+1)))
    elif kind == 'blackman': a = np.pi*u/(W2+1); k = 0.42 + 0.5*np.cos(a) + 0.08*np.cos(2*a)
    return k/k.sum()
def predict(ev, W1, W2, kind, shift):
    n = 600; x = np.full(n, dc); x[300:300+len(ev)] += ev
    T = law_db(np.abs(x)) - law_db(dc)               # target reduction (dB, <= 0); true peak == sample here (DC + block)
    m = -(minimum_filter1d(-(-T), 1) ) if False else minimum_filter1d(T, size=2*W1+1, mode='nearest')
    g = np.convolve(m, kernel(kind, W2), mode='same')
    return np.roll(g, shift)[300-150:300+len(ev)+150]
best = []
for kind in ('box', 'tri', 'hann', 'blackman'):
    for W1 in range(0, 48):
        for W2 in range(4, 64):
            if W1 + W2 > 70: continue
            for shift in (-2, -1, 0, 1, 2):
                err = []
                for name, ev in cases.items():
                    ref = D[f'{name}|ref'][s-150:s+len(ev)+150]
                    p = predict(np.array(ev), W1, W2, kind, shift)
                    err.append(p - ref)
                e = np.concatenate(err); r = np.sqrt(np.mean(e**2))
                best.append((r, kind, W1, W2, shift))
best.sort()
for b in best[:8]: print('rms %.3f dB  kernel %-8s W1 %2d W2 %2d shift %d' % b)
