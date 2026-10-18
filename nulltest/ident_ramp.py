"""Sample-accurate gain around events, on a DC carrier (private)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wvox as N
FS = 48000; c = -24; M = 10**(-c/20)
def law_db(x): return 20*np.log10(0.99*M/(1+(M-1)*x))
dc = 0.01
x0 = np.full(int(1.5*FS), dc, np.float32)
y0 = R.render(x0, FS, comp=c)[0]
print('DC carrier: gain %.3f dB, static law %.3f dB, ripple %.2e' % (20*np.log10(y0[FS]/dc), law_db(dc), np.ptp(y0[FS:FS+1000])))
OUT = {}
for name, ev in (('impulse 1 smp +0.5', [0.5]), ('block 24 smp +0.3', [0.3]*24), ('step +0.3 for 300 ms', [0.3]*int(0.3*FS))):
    x = x0.copy(); s = FS
    x[s:s+len(ev)] += np.array(ev, np.float32)
    for who in ('ref', 'ours'):
        y = R.render(x, FS, comp=c)[0] if who == 'ref' else N.ours(x, FS, c, None, 0)[0]
        g = 20*np.log10(np.abs(y/x))
        g0 = np.median(g[s-2000:s-500])
        OUT[(name, who)] = g - g0
        pre = [(-k, round(float(g[s-k]-g0), 2)) for k in (96, 64, 48, 40, 32, 24, 16, 12, 8, 4, 1)]
        e = s + len(ev)
        post = [(k, round(float(g[e+k]-g0), 2)) for k in (0, 4, 8, 16, 24, 32, 48, 64, 96, 192, 480, 960)]
        print(f'{who:4s} {name:22s} before start: {pre}')
        print(f'{"":4s} {"":22s} after end:    {post}')
np.savez(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'ramp.npz'), **{f'{k[0]}|{k[1]}': v for k, v in OUT.items()})
