import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
FS = 48000; S = N.signals()
names = ["white noise -20 rms", "pink noise -20 rms", "voice-like", "tone steps -50/-10/-30", "noise bursts", "AM 5 Hz 3 kHz"]
REF = {(n, c): R.render(S[n], FS, comp=c)[0] for n in names for c in (-12, -24, -36)}
base = dict(atkMs=154.7, relA=7.374, relB=0.506, relQ=1.636, windowMs=0.348, slowWindowMs=8)
def run(**kw):
    d = dict(base); d.update(kw)
    tune = tuple(f'{k}={v}' for k, v in d.items())
    out = []
    for n in names:
        row = [N.residual_db(N.ours(S[n], FS, c, None, 0, tune)[0], REF[(n, c)], int(0.2*FS)) for c in (-12, -24, -36)]
        out.append(row)
    return np.array(out)
for kw in sys.argv[1:]:
    k, vs = kw.split('=')
    for v in vs.split(','):
        r = run(**{k: float(v)})
        print(f'{k}={v:>7}  mean {r.mean():6.1f} | ' + '  '.join(f'{n[:9]}:{r[i].mean():6.1f}' for i, n in enumerate(names)), flush=True)
