import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; c = -24
def xdet(g_db):
    M = 10**(-c/20); g = 10**(g_db/20); return (0.99*M/g - 1)/(M - 1)
def traj(x):
    y = R.render(x.astype(np.float32), FS, comp=c, preroll=0.1)[0]
    w = 16                                                   # 1/3 ms windows
    e = lambda s: np.abs(s[:len(s)//w*w]).reshape(-1, w).max(axis=1)
    ei, eo = e(x), e(y)
    g = 20*np.log10(eo/np.maximum(ei, 1e-9))
    xd = np.maximum(xdet(g), 1e-6)
    return 20*np.log10(np.maximum(ei, 1e-6)), 20*np.log10(xd), w/FS
t = np.arange(int(3*FS))/FS
cases = {}
# steps down
cases['step -10->-50'] = np.concatenate([R.tone(FS, 3000, 0.5, -10), R.tone(FS, 3000, 2.5, -50)])
cases['step -10->-30'] = np.concatenate([R.tone(FS, 3000, 0.5, -10), R.tone(FS, 3000, 2.5, -30)])
cases['step -10->-20'] = np.concatenate([R.tone(FS, 3000, 0.5, -10), R.tone(FS, 3000, 2.5, -20)])
cases['step -30->-50'] = np.concatenate([R.tone(FS, 3000, 0.5, -30), R.tone(FS, 3000, 2.5, -50)])
for fm, d in ((2, 0.5), (5, 0.5), (10, 0.9)):
    cases[f'AM {fm}Hz {d}'] = 0.1*(1+d*np.sin(2*np.pi*fm*t))*np.sin(2*np.pi*3000*t)
res = {}
for k, x in cases.items():
    e_db, x_db, dt = traj(x)
    gap = x_db - e_db
    # rate of fall of x (dB/s), smoothed over 6 windows (2 ms)
    k6 = 6
    rate = -(x_db[k6:] - x_db[:-k6])/(k6*dt)
    gap_m = 0.5*(gap[k6:] + gap[:-k6])
    sel = (gap_m > 0.2) & (np.arange(len(rate)) > 3000)
    res[k] = (gap_m[sel], rate[sel])
bins = [0.5, 1, 2, 3, 5, 8, 12, 18, 25, 35, 45]
print('median fall rate of the detector level (dB/s) by gap (detector level above input envelope, dB)')
print('%-16s' % 'case', *['%6s' % ('%g' % b) for b in bins])
for k, (g, r) in res.items():
    row = []
    for lo, hi in zip(bins[:-1], bins[1:]):
        m = (g >= lo) & (g < hi)
        row.append('%6.0f' % np.median(r[m]) if m.sum() > 6 else '     -')
    print('%-16s' % k, '     ', *row)
