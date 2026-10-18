import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
FS = 48000; S = N.signals()
names = ["white noise -20 rms", "pink noise -20 rms", "voice-like", "tone steps -50/-10/-30", "noise bursts", "AM 5 Hz 3 kHz", "log sweep -20"]
comps = (-12, -24, -36)
REF = {(n, c): R.render(S[n], FS, comp=c)[0] for n in names for c in comps}
def run(**kw):
    tune = tuple(f'{k}={v}' for k, v in kw.items())
    return np.array([[N.residual_db(N.ours(S[n], FS, c, None, 0, tune)[0], REF[(n, c)], int(0.2*FS)) for c in comps] for n in names])
if __name__ == '__main__':
    key = sys.argv[1]; vals = sys.argv[2].split(',')
    extra = tuple(a.split('=') for a in sys.argv[3:])
    print('%-12s' % key + ''.join('%11s' % n[:10] for n in names) + '   mean')
    for v in vals:
        kw = {key: float(v)}; kw.update({k: float(x) for k, x in extra})
        r = run(**kw)
        print('%-12s' % v + ''.join('%11.1f' % r[i].mean() for i in range(len(names))) + '   %.1f' % r.mean(), flush=True)
