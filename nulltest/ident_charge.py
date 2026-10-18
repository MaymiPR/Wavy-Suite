import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000; c = -24; M = 10**(-c/20)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'charge.npz')
def xdet_db(g_db): g = 10**(g_db/20); return 20*np.log10(max((0.99*M/g - 1)/(M - 1), 1e-9))
durs = [2, 5, 10, 20, 50, 100, 200, 400, 800]
levels = [-5, -10, -20, -30]
bases = [-60, -45]
res = {}
for base in bases:
    for L in levels:
        for d in durs:
            x = np.concatenate([R.tone(FS, 3000, 0.4, base), R.tone(FS, 3000, d/1000, L), R.tone(FS, 3000, 1.5, base)])
            y = R.render(x, FS, comp=c)[0]
            w = 16
            t0 = int((0.4 + d/1000)*FS)
            # read the level 1.5 ms after the burst, and then at several later times for the release shape
            row = []
            for dt in (1.5, 10, 30, 100, 300, 800):
                i = t0 + int(dt/1000*FS)
                seg = slice(i, i + w)
                g = 20*np.log10(np.abs(y[seg]).max()/np.abs(x[seg]).max())
                row.append(xdet_db(g))
            res[(base, L, d)] = row
        print(base, L, flush=True)
keys = sorted(res); np.savez(OUT, keys=np.array(keys), vals=np.array([res[k] for k in keys]))
print('base -60, L -10:'); 
for d in durs: print(d, np.round(res[(-60, -10, d)], 1))
