"""One-parameter sweeps of the WComp null from a base setting (to find which control the weak nulls come from).
    python -I sweep_wcomp.py [threshold ratio attack release punch]     (base; default -24 4 7 100 1)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wcomp as N, nulltest_wvox as NV, refhost as R

fs = 48000
S = N.signals(fs)
sig = {k: S[k] for k in ('voice-like', 'drum loop', 'bass line', 'pink noise -20 rms')}
base = [float(a) for a in sys.argv[1:6]] if len(sys.argv) > 5 else [-24.0, 4.0, 7.0, 100.0, 1.0]
sweeps = {'punch': (4, [0, 1, 3, 6, 10, 15, 30]), 'attack': (2, [0.5, 2, 7, 15, 30, 60, 100]),
          'release': (3, [3, 10, 30, 100, 300, 1000, 3000]), 'ratio': (1, [1.5, 2, 4, 10, 20, 50]), 'threshold': (0, [-6, -12, -24, -36, -48])}
for name, (idx, vals) in sweeps.items():
    print('== %s (base thr %g ratio %g att %g rel %g punch %g)' % (name, *base), flush=True)
    for v in vals:
        st = list(base); st[idx] = v
        row = []
        for sn, x in sig.items():
            xs = np.stack([x, x]).astype(np.float32)
            R.fresh('H-Comp Stereo')
            a = N.ours(xs, fs, *st, 100.0, 0.0, 'Off'); b = N.theirs(xs, fs, *st, 100.0, 0.0, 'Off')
            row.append(NV.residual_db(a[0], b[0], int(0.5 * fs)))
        print('  %-9s %7g:  voice %6.1f  drums %6.1f  bass %6.1f  noise %6.1f' % (name, v, *row), flush=True)
