"""Renders transient material (the null test's kick-like and bass-notes signals, plus a fast bass riff) through the
reference at several Freq settings, generated part split off with the Intensity render (-24 dB, where the limiter
never acts) and the limited Intensity 0 render kept too. -> measurements/rbass/transients.npz (git-ignored)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, refhost as R, nulltest_wbass as N, wbass_model as M
from scipy.signal import sosfilt
FS = 48000
S = N.signals()
t = np.arange(int(2.0 * FS)) / FS
riff = np.zeros_like(t)
for k, f in enumerate((55, 55, 82.4, 73.4, 41.2, 49, 61.7, 55)):
    s0 = int(k * 0.25 * FS); m = int(0.22 * FS); tt = np.arange(m) / FS
    riff[s0:s0 + m] += 0.6 * np.exp(-tt / 0.12) * np.sin(2 * np.pi * f * tt) * np.minimum(1, tt / 0.002)
sig = {"kick": S["kick-like"], "notes": S["bass notes"], "riff": riff.astype(np.float32)}
out = {}
for F in (80, 160, 256):
    for name, x in sig.items():
        x = np.asarray(x, np.float32)
        hp = sosfilt(M.sos(3, 'high', F), x.astype(np.float64))
        lo = R.render_rbass(x, FS, freq=F, intensity=-24, hp=1)[0].astype(np.float64)
        y0 = R.render_rbass(x, FS, freq=F, intensity=0, hp=1)[0].astype(np.float64)
        out["x_%s_%d" % (name, F)] = x
        out["lin_%s_%d" % (name, F)] = (lo - hp) * 10 ** (24 / 20)     # unlimited generated part at Intensity 0
        out["lim_%s_%d" % (name, F)] = y0 - hp                          # what the reference outputs (limited)
        print(F, name, "peak unlimited %.2f" % np.max(np.abs(out["lin_%s_%d" % (name, F)])), flush=True)
np.savez_compressed(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "transients.npz"), **out)
