"""Tune the gain-smoothing constants of WVox against the reference (private; uses reference renders).

    python -I tune_wvox.py [maxiter]
"""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
from scipy.optimize import minimize
FS = 48000
S = N.signals()
t = np.arange(int(3*FS))/FS
for lo, hi in ((-10, -30), (-20, -50), (-5, -15)):
    S[f"step {lo}/{hi}"] = np.concatenate([R.tone(FS, 3000, 1.0, lo), R.tone(FS, 3000, 2.0, hi)])
parts = [R.tone(FS, 3000, 0.3, -50)]
for dur in (5, 20, 50, 200):
    parts += [R.tone(FS, 3000, dur/1000, -10), R.tone(FS, 3000, 0.5, -50)]
S['bursts -10 dB of 5/20/50/200 ms'] = np.concatenate(parts)
S["sine 8 kHz -20"] = R.tone(FS, 8000, 3.0, -20)
# signals that cross the gate threshold
S["gate sweep tones"] = np.concatenate([R.tone(FS, 1000, 0.6, L) for L in (-60, -45, -35, -25, -45)])
plain = [(-12, None, 0), (-24, None, 0), (-36, None, 0)]
gated = [(-24, -40, 0), (-12, -30, -3)]
cases = []
for nme in ["tone steps -50/-10/-30", "AM 5 Hz 3 kHz", "white noise -20 rms", "pink noise -20 rms", "noise bursts", "voice-like", "log sweep -20",
            "step -10/-30", "step -20/-50", "step -5/-15", "bursts -10 dB of 5/20/50/200 ms", "sine 100 Hz -20"]:
    cases += [(nme, s) for s in plain]
for nme in ["tone steps -50/-10/-30", "white noise -20 rms", "voice-like", "gate sweep tones", "noise bursts"]:
    cases += [(nme, s) for s in gated]
REF = {}
for nme, (c, g, gn) in cases:
    REF[(nme, c, g, gn)] = R.render(S[nme], FS, comp=c, gate=g, gain=gn)[0]
print('reference renders cached', len(REF), R.STATS, flush=True)
DOMAIN = os.environ.get('WVOX_DOMAIN', '0')
KEYS = os.environ.get('WVOX_KEYS', 'ta1 tr1 ta2 tr2').split()
def score(p, verbose=False):
    if min(p) < 0.3 or max(p) > 5000: return 100.0
    tune = tuple(f'{k}={v:.5g}' for k, v in zip(KEYS, p)) + tuple(e for e in os.environ.get('WVOX_EXTRA', '').split() if e)
    tot = []
    for nme, (c, g, gn) in cases:
        mine = N.ours(S[nme], FS, c, g, gn, tune)[0]
        r = N.residual_db(mine, REF[(nme, c, g, gn)], int(0.2*FS))
        tot.append(r)
        if verbose: print('%-34s %4d %5s %7.1f' % (nme, c, 'off' if g is None else g, r))
    return float(np.mean(tot))
if __name__ == '__main__':
    p0 = [93.0, 15.0, 83.0, 439.0]
    if len(sys.argv) > 2: p0 = [float(v) for v in sys.argv[2].split(',')]
    print('start', score(p0), flush=True)
    r = minimize(score, p0, method='Nelder-Mead', options={'xatol': 1e-2, 'fatol': 0.01, 'maxiter': int(sys.argv[1]) if len(sys.argv) > 1 else 500, 'adaptive': True})
    print('best', ','.join('%.5g' % v for v in r.x), r.fun)
    print(dict(zip(KEYS, np.round(r.x, 4))))
    score(r.x, True)
