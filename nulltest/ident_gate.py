"""Gate gain trajectory, sample-accurate on a DC carrier (private)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R
FS = 48000
def run(levels_db, durs, gate, comp=0):
    x = np.concatenate([np.full(int(d*FS), 10**(L/20), np.float32) for L, d in zip(levels_db, durs)])
    y = R.render(x, FS, comp=comp, gate=gate)[0]
    return 20*np.log10(np.abs(y/x)) + 0.0873, x
out = {}
for name, lv in (('-20 -> -50 -> -20', (-20, -50, -20)), ('-20 -> -70 -> -20', (-20, -70, -20)), ('-20 -> -42 -> -20', (-20, -42, -20))):
    g, x = run(lv, (1.0, 1.5, 0.5), -40)
    out[name] = g
    t1 = FS; t2 = int(2.5*FS)
    print(name, 'closing (ms after the drop):', [(ms, round(float(g[t1 + int(ms*48)]), 2)) for ms in (0, 1, 2, 5, 10, 20, 50, 100, 200, 300, 400, 500, 600, 800, 1000, 1400)])
    print(' ' * len(name), 'opening (samples around the rise):', [(k, round(float(g[t2 + k]), 2)) for k in (-96, -64, -48, -32, -16, -8, -1, 0, 8, 32, 64, 128)])
np.savez(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'gate_traj.npz'), **out)
