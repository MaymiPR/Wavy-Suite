"""Compares the sample-wise gain of WComp and H-Comp on one setting and signal: g = y / x(n - 64) where the delayed input
is loud enough, in dB, over time; writes build/meters/gain_<tag>.png. Private diagnostic.
    python -I gain_diag.py <tag> <signal> thr ratio attack release punch [t0 t1]"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
import nulltest_wcomp as N, nulltest_wvox as NV, refhost as R

fs = 48000; LA = 64
tag, sname = sys.argv[1], sys.argv[2]
thr, rat, att, rel, pu = [float(v) for v in sys.argv[3:8]]
t0, t1 = (float(sys.argv[8]), float(sys.argv[9])) if len(sys.argv) > 9 else (0.5, 2.5)
x = N.signals(fs)[sname]; xs = np.stack([x, x]).astype(np.float32)
R.fresh('H-Comp Stereo')
a = N.ours(xs, fs, thr, rat, att, rel, pu, 100.0, 0.0, 'Off')[0]; b = N.theirs(xs, fs, thr, rat, att, rel, pu, 100.0, 0.0, 'Off')[0]
print('null %.1f dB' % NV.residual_db(a, b, int(0.5 * fs)))
xd = np.r_[np.zeros(LA), x[:-LA]]
ok = np.abs(xd) > 0.08 * np.abs(x).max()
t = np.arange(len(x)) / fs
gA = 20 * np.log10(np.abs(a[ok] / xd[ok]) + 1e-9); gB = 20 * np.log10(np.abs(b[ok] / xd[ok]) + 1e-9)
fig, ax = plt.subplots(3, 1, figsize=(14, 9), sharex=True)
ax[0].plot(t, x, lw=0.4, c='0.5'); ax[0].set_ylabel('input')
ax[1].plot(t[ok], gB, ',', c='C0', label='H-Comp'); ax[1].plot(t[ok], gA, ',', c='C3', label='WComp'); ax[1].set_ylabel('gain dB'); ax[1].legend()
ax[2].plot(t[ok], gA - gB, ',', c='k'); ax[2].set_ylabel('WComp - H-Comp dB'); ax[2].set_ylim(-6, 6); ax[2].axhline(0, c='0.7')
ax[2].set_xlim(t0, t1)
s = ok & (t > 0.5); e = (20 * np.log10(np.abs(a[s] / xd[s]) + 1e-9) - 20 * np.log10(np.abs(b[s] / xd[s]) + 1e-9))
print('gain error dB: mean %+.2f, rms %.2f' % (e.mean(), np.sqrt(np.mean(e ** 2))))
plt.tight_layout(); plt.savefig(os.path.join('..', 'build', 'meters', 'gain_%s.png' % tag), dpi=80)
