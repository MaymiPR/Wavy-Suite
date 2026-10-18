"""Fits the reference meter law to a tools/MeterCapture recording (frames + output audio):
bar(dB) = env, env rises at once to the peak of the last `win` seconds and otherwise moves toward it as a one-pole on
linear amplitude with time constant `tau`; the editor shows it `delay` seconds late; px = off + ppd * dB.

    python -I fit_meter.py build/meters/rbp.raw
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import least_squares
from scipy.ndimage import maximum_filter1d
import meter_frames as MF

FS = 48000
HOP = 48            # 1 ms model steps


def envelope(y, tau, win):
    a = np.abs(y)
    n = len(a) // HOP
    hop_pk = a[:n * HOP].reshape(n, HOP).max(1)
    w = max(1, int(round(win * 1000)))
    pk = maximum_filter1d(hop_pk, w, origin=(w - 1) // 2)       # peak over the last `win` seconds
    k = 1 - np.exp(-HOP / FS / tau)
    env = np.empty(n); e = 0.0
    for i in range(n):
        p = pk[i]
        e = p if p > e else e + (p - e) * k
        env[i] = e
    return env


def model(params, t, y, track):
    tau, win, delay, ppd, off = params
    env = envelope(y, tau, win)
    idx = np.clip(((t - delay) * 1000).astype(int), 0, len(env) - 1)
    db = 20 * np.log10(np.maximum(env[idx], 1e-6))
    return np.clip(off + ppd * db, 0, track)


if __name__ == "__main__":
    t, bars, out = MF.load(sys.argv[1])
    y = out[:, 0]
    L = bars["L"].astype(float) - 2           # the gain knob covers 2 px at the top
    m = (L > 4) & (L < 140)
    def res(p): return (model(p, t, y, 1e9) - L)[m]
    best = None
    for tau0 in (0.2, 0.33, 0.5):
        for win0 in (0.005, 0.02, 0.05):
            r = least_squares(res, [tau0, win0, 0.03, 5.17, 143.0], bounds=([0.01, 0.001, -0.1, 3, 100], [3, 0.3, 0.3, 8, 200]),
                              diff_step=[1e-2, 5e-2, 1e-2, 1e-3, 1e-3])
            if best is None or r.cost < best.cost: best = r
    tau, win, delay, ppd, off = best.x
    e = res(best.x)
    print("tau %.3f s (fall %.1f dB/s in silence)  win %.1f ms  delay %.1f ms  %.3f px/dB  0 dB at %.1f px   rms %.2f px, max %.1f px"
          % (tau, 8.686 / tau, win * 1000, delay * 1000, ppd, off, np.sqrt(np.mean(e ** 2)), np.abs(e).max()))
