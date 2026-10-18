"""Dense static law of the reference: gain (dB) for a 1 kHz sine at peak level L (dBFS) and Compression c (dB).
Private data: written to measurements/rvox/static_dense.npz (git-ignored)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000
cs = np.arange(0, -36.01, -1.0)
Ls = np.arange(-100, 12.01, 1.0)
G = np.zeros((len(cs), len(Ls)))
for i, c in enumerate(cs):
    for j, L in enumerate(Ls):
        x = R.tone(FS, 1000, 0.8, L)
        y = R.render(x, FS, comp=c)[0]
        n = int(0.3 * FS)
        G[i, j] = R.rms_db(y[-n:]) - R.rms_db(x[-n:])
    print(c, flush=True)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'static_dense.npz')
np.savez(out, cs=cs, Ls=Ls, G=G)
print('retries', R.STATS)
