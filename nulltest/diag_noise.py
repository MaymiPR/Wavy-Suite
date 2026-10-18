import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
FS = 48000; c = -24; M = 10**(-c/20)
rng = np.random.default_rng(3); n = 3*FS
z = rng.standard_normal(n); x = (z/z.std()*10**(-20/20)).astype(np.float32)
ref = R.render(x, FS, comp=c)[0]; ours = N.ours(x, FS, c, None, 0)[0]
m = np.abs(x) > 1e-3
def lev(y):
    g = np.full(n, np.nan); g[m] = np.abs(y[m]/x[m])
    L = (0.99*M/g - 1)/(M - 1)
    return 20*np.log10(np.maximum(L, 1e-6))
Lr, Lo = lev(ref), lev(ours)
s = slice(FS, None)
d = Lo[s] - Lr[s]; d = d[np.isfinite(d)]
print('detected level (dB) ref mean %.2f std %.2f | ours mean %.2f std %.2f | diff mean %+.2f std %.2f' % (np.nanmean(Lr[s]), np.nanstd(Lr[s]), np.nanmean(Lo[s]), np.nanstd(Lo[s]), d.mean(), d.std()))
print('ref level percentiles 5/50/95:', np.round(np.nanpercentile(Lr[s], [5, 50, 95]), 2), ' ours:', np.round(np.nanpercentile(Lo[s], [5, 50, 95]), 2))
# compare ref level with running peak windows of |x|
from scipy.ndimage import maximum_filter1d
ax = np.abs(x).astype(float)
for w in (48, 240, 480, 960, 2400, 4800):
    pk = 20*np.log10(np.maximum(maximum_filter1d(ax, w, origin=(w-1)//2), 1e-9))   # trailing window
    dd = (Lr - pk)[s]; dd = dd[np.isfinite(dd)]
    print('ref level minus trailing peak over %4d smp: mean %+.2f std %.2f' % (w, dd.mean(), dd.std()))
np.savez(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'diag_noise.npz'), x=x, ref=ref, ours=ours)
