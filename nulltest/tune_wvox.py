"""Tune the detector constants of WVox against the reference (private; uses reference renders)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
from scipy.optimize import minimize
FS = 48000
S = N.signals()
train_names = ["tone steps -50/-10/-30", "AM 5 Hz 3 kHz", "white noise -20 rms", "pink noise -20 rms", "noise bursts", "voice-like", "log sweep -20"]
extra = {}
t = np.arange(int(3*FS))/FS
for lo, hi in ((-10, -30), (-20, -50), (-5, -15)):
    extra[f"step {lo}/{hi}"] = np.concatenate([R.tone(FS, 3000, 1.0, lo), R.tone(FS, 3000, 2.0, hi)])
parts = [R.tone(FS, 3000, 0.3, -50)]
for dur in (5, 20, 50, 200):
    parts += [R.tone(FS, 3000, dur/1000, -10), R.tone(FS, 3000, 0.5, -50)]
extra['bursts -10 dB of 5/20/50/200 ms'] = np.concatenate(parts)
extra["sine 8 kHz -20"] = R.tone(FS, 8000, 3.0, -20)
extra["sine 100 Hz -20"] = R.tone(FS, 100, 3.0, -20)
S.update(extra)
names = train_names + list(extra)
sets = [(-12, None, 0), (-24, None, 0), (-36, None, 0)]
REF = {}
for nme in names:
    for c, g, gn in sets:
        REF[(nme, c)] = R.render(S[nme], FS, comp=c, gate=g, gain=gn)[0]
print('reference renders cached', len(REF), R.STATS, flush=True)
KEYS = ['atkMs', 'relA', 'relB', 'relQ', 'windowMs', 'slowWindowMs']
def score(p, verbose=False):
    if p[0] < 5 or p[0] > 600 or p[1] <= 0 or p[2] <= 0.5 or p[3] < 0.2 or p[3] > 2.5 or p[4] < 0.04 or p[4] > 3 or p[5] < 0.5 or p[5] > 60: return 100.0
    tune = tuple(f'{k}={v:.5g}' for k, v in zip(KEYS, p))
    tot = []
    for nme in names:
        for c, g, gn in sets:
            mine = N.ours(S[nme], FS, c, g, gn, tune)[0]
            r = N.residual_db(mine, REF[(nme, c)], int(0.2*FS))
            tot.append(r)
            if verbose: print('%-26s %4d %7.1f' % (nme, c, r))
    return float(np.mean(tot))
p0 = [93.07, 5.0674, 0.5022, 1.9327, 0.5191, 12.2231]
print('start', score(p0))
r = minimize(score, p0, method='Nelder-Mead', options={'xatol': 1e-2, 'fatol': 0.02, 'maxiter': 200, 'initial_simplex': None})
print('best', dict(zip(KEYS, np.round(r.x, 4))), r.fun)
score(r.x, True)
