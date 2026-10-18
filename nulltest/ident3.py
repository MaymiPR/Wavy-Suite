import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000
def xdet(gain_db, c):
    M = 10**(-c/20); g = 10**(gain_db/20)
    return (0.99*M/g - 1)/(M - 1)
def measure(x, c=-24):
    y = R.render(x.astype(np.float32), FS, comp=c)[0]
    n = FS//2
    g = R.rms_db(y[-n:]) - R.rms_db(x[-n:])
    return xdet(g, c)
n = FS; t = np.arange(n)/FS; ph = (t*1000) % 1.0
A = 0.1
rng = np.random.default_rng(5)
S = {}
S['sine'] = A*np.sin(2*np.pi*ph)
S['triangle'] = A*(2/np.pi)*np.arcsin(np.sin(2*np.pi*ph))
S['saw'] = A*(2*ph-1)
for d in (0.5, 0.3, 0.2, 0.1, 0.05):
    S[f'pulse_uni_{d}'] = A*(ph < d).astype(float)
    S[f'pulse_bi_{d}'] = A*np.where(ph < d, 1.0, -d/(1-d))      # zero-mean pulse train, peak A
S['half_sine'] = A*np.maximum(np.sin(2*np.pi*ph), 0)
S['gauss'] = (lambda z: z*A/3.0/z.std())(rng.standard_normal(n))
S['uniform'] = A*(2*rng.random(n)-1)
print('%-16s %7s %7s %7s %9s %9s' % ('signal', 'peak', 'rms', 'meanabs', 'xdet', 'xdet/peak(dB)'))
for k, x in S.items():
    pk = np.abs(x).max(); rms = np.sqrt(np.mean(x*x)); ma = np.mean(np.abs(x))
    xd = measure(x)
    print('%-16s %7.4f %7.4f %7.4f %9.5f %9.2f   rms*sqrt2=%.4f' % (k, pk, rms, ma, xd, 20*np.log10(xd/pk), rms*np.sqrt(2)))
