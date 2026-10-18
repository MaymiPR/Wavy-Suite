"""Fits the RVox Energy meter (input level) to a tools/MeterCapture recording: frame peak of the input, then one or
two one-pole stages on linear amplitude (instant rise) with time constants tau1, tau2; px = off + ppd * dB.

    python -I fit_meter_energy.py build/meters/rv_probe.raw build/meters/probe.f32 [more pairs ...]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.optimize import least_squares
import meter_frames as MF

FS = 48000; HOP = 48


def model(x, tau1, tau2, win=0.017):
    a = np.abs(x); n = len(a) // HOP
    hp = a[:n * HOP].reshape(n, HOP).max(1)
    w = max(1, int(win * 1000))
    from scipy.ndimage import maximum_filter1d
    pk = maximum_filter1d(hp, w, origin=(w - 1) // 2)
    k1 = 1 - np.exp(-HOP / FS / tau1); k2 = 1 - np.exp(-HOP / FS / tau2) if tau2 > 0 else 1.0
    e1 = e2 = 0.0; out = np.empty(n)
    for i, p in enumerate(pk):
        e1 = p if p > e1 else e1 + (p - e1) * k1
        e2 = e1 if e1 > e2 else e2 + (e1 - e2) * k2
        out[i] = e2
    return out


if __name__ == "__main__":
    sets = []
    for cap, inp in zip(sys.argv[1::2], sys.argv[2::2]):
        t, bars, _ = MF.load(cap, "vox")
        x = np.fromfile(inp, dtype=np.float32).astype(float)
        E = bars["E"].astype(float)
        sets.append((t, x, E, (E > 5) & (E < 140) & (t > 0.3)))
    for two in (False, True):
        def res(p):
            tau1, tau2, delay, ppd, off = p
            r = []
            for t, x, E, m in sets:
                env = model(x, tau1, tau2 if two else 0)
                idx = np.clip(((t - delay) * 1000).astype(int), 0, len(env) - 1)
                r.append((np.clip(off + ppd * 20 * np.log10(np.maximum(env[idx], 1e-6)), 0, 145) - E)[m])
            return np.concatenate(r)
        best = None
        for t1 in (0.15, 0.3, 0.5):
            for t2 in (0.05, 0.15):
                r = least_squares(res, [t1, t2, 0.02, 1.94, 147], bounds=([0.01, 0.005, -0.1, 1, 100], [3, 2, 0.3, 4, 200]), diff_step=1e-2)
                if best is None or r.cost < best.cost: best = r
        e = res(best.x)
        print("%s: tau1 %.3f s  tau2 %.3f s  delay %.0f ms  %.3f px/dB  0 dB at %.1f px   rms %.2f px" % (
            "two stages" if two else "one stage", best.x[0], best.x[1] if two else 0, best.x[2] * 1000, best.x[3], best.x[4], np.sqrt(np.mean(e ** 2))))
