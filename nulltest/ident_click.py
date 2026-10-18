"""Gain after a short loud event riding on a steady quiet tone (private): fast-path recovery."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
FS = 48000; c = -24
base = R.tone(FS, 3000, 2.0, -40).astype(np.float64)
w = 16
def gain_track(y, x):
    a = np.abs(y[:len(y)//w*w]).reshape(-1, w).max(axis=1); b = np.abs(x[:len(x)//w*w]).reshape(-1, w).max(axis=1)
    return 20*np.log10(a/b)
times_ms = [-1, 0.5, 1, 2, 3, 5, 8, 12, 20, 30, 50, 80, 120, 200, 400]
print('gain change (dB) after an event at t=1.0 s; times in ms after the event end')
print('%-28s' % 'event' + ''.join('%7g' % t for t in times_ms))
for n_samp, lvl in ((3, -6), (24, -6), (240, -6), (2400, -6), (24, -20), (240, -20)):
    x = base.copy(); s = FS
    ev = 10**(lvl/20)*np.sin(2*np.pi*12000*np.arange(n_samp)/FS)
    x[s:s+n_samp] += ev
    x = x.astype(np.float32)
    rows = {}
    for who in ('ref', 'ours'):
        y = R.render(x, FS, comp=c)[0] if who == 'ref' else N.ours(x, FS, c, None, 0)[0]
        g = gain_track(y, base.astype(np.float32))       # gain relative to the underlying tone (event windows excluded below)
        g0 = np.median(g[int(0.9*FS/w):int(0.99*FS/w)])
        end = s + n_samp
        vals = []
        for tm in times_ms:
            i = int((end + tm/1000*FS)/w)
            vals.append(g[i] - g0)
        rows[who] = vals
        print('%-28s' % (f'{who}: {n_samp} smp @{lvl} dB') + ''.join('%7.2f' % v for v in vals), flush=True)
