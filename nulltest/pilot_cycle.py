"""The gain inside each cycle of a low tone, H-Comp against WComp: a -40 dBFS 6170 Hz pilot on the tone, demodulated
from input and output (64-sample latency removed, 1.5 kHz low-pass). Writes build/meters/pilot_cycle_<tag>.png and
prints the mean gain. Private diagnostic.
    python -I pilot_cycle.py <tag> <tone Hz> <tone dBFS> thr ratio attack release punch"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
import nulltest_wcomp as N, refhost as R

fs, F0, LA = 48000, 6170.0, 64
tag = sys.argv[1]; f, db = float(sys.argv[2]), float(sys.argv[3]); st = [float(v) for v in sys.argv[4:9]]
t = np.arange(int(2.5 * fs)) / fs
x = (10 ** (db / 20) * np.sin(2 * np.pi * f * t) + 0.01 * np.sin(2 * np.pi * F0 * t)).astype(np.float32); xs = np.stack([x, x])
R.fresh('H-Comp Stereo')
outs = {'H-Comp': N.theirs(xs, fs, *st, 100.0, 0.0, 'Off')[0], 'WComp': N.ours(xs, fs, *st, 100.0, 0.0, 'Off')[0]}
ph = np.exp(-2j * np.pi * F0 * t)
def lp(v):
    V = np.fft.rfft(v); fr = np.fft.rfftfreq(len(v), 1 / fs); V[fr > 1500] = 0; return np.fft.irfft(V, len(v))
dem = lambda v: np.abs(lp((v * ph).real) + 1j * lp((v * ph).imag))
dx = dem(x)
fig, ax = plt.subplots(figsize=(12, 5))
s = (t > 2.0) & (t < 2.0 + 3 / f)
for k, y in outs.items():
    g = 20 * np.log10(dem(np.r_[y[LA:], np.zeros(LA)]) / dx)
    ax.plot(t[s], g[s], label='%s (mean over 1.5-2.5 s %.2f dB)' % (k, np.mean(g[(t > 1.5) & (t < 2.45)])))
    print(k, 'mean gain %.2f dB, range %.2f .. %.2f in a cycle' % (np.mean(g[(t > 1.5) & (t < 2.45)]), g[s].min(), g[s].max()))
ax.plot(t[s], -20 + 10 * np.abs(x[s]) / np.abs(x).max(), c='0.75', label='|tone| (scaled)')
ax.set_title('%g Hz %g dBFS, thr %g ratio %g attack %g release %g punch %g' % (f, db, *st)); ax.set_ylabel('gain dB (incl. makeup)'); ax.legend()
plt.tight_layout(); plt.savefig('../build/meters/pilot_cycle_%s.png' % tag, dpi=75)
