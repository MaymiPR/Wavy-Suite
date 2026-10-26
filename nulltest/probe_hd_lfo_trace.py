"""Probe: the delay's trace d(t) under modulation, from the phase of a steady sine through the wet path (Mix 100,
Feedback 0), the reference against WDelayRender.

    python -I probe_hd_lfo_trace.py ms depth rate [extra WDelayRender args]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal
import nulltest_wdelay as N

def trace(y, x, f, fs):
    a = np.unwrap(np.angle(signal.hilbert(y))); b = np.unwrap(np.angle(signal.hilbert(x)))
    return (b - a) / (2 * np.pi * f) * fs

if __name__ == "__main__":
    fs = int(os.environ.get("FS", "48000")); f = 300.0
    ms, d, rt = map(float, sys.argv[1:4])
    t = np.arange(int(2.0 * fs)) / fs
    x = (0.3 * np.sin(2 * np.pi * f * (t + 3.0))).astype(np.float32); x2 = np.stack([x, x])
    k = dict(analog="Off", feedback=0, mix=100, depth=d, rate=rt, preroll=3.0, changes=[(-2.5, "sync", "ms"), (-2.45, "delay", ms), (0.0, "feedback", 0)])
    r = N.theirs(x2, fs, **k)
    N.EXTRA = sys.argv[4:]
    o = N.ours(x2, fs, **k)
    dr, do = trace(r[0], x, f, fs), trace(o[0], x, f, fs)
    # align the unwrapped offsets by whole periods of the sine
    P = fs / f; do += P * np.round(np.median(dr[2000:-2000] - do[2000:-2000]) / P)
    np.save(os.path.join(N.TMP, 'lfotrace.npy'), np.stack([dr, do]))
    s = slice(2000, len(x) - 2000)
    print('ref d: min %.2f max %.2f  ours: min %.2f max %.2f  diff rms %.4f max %.4f samples' % (dr[s].min(), dr[s].max(), do[s].min(), do[s].max(), np.sqrt(np.mean((dr[s] - do[s]) ** 2)), np.abs(dr[s] - do[s]).max()))
