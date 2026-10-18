"""Tries rules for the reference RVox output meter against a tools/MeterCapture recording (frames + output audio).
The bar shows the frame's peak p while it is above a falling line; the line falls at `rate` dB/s from a hold value,
and the rules differ in when the hold restarts.

    python -I fit_meter_vox.py build/meters/rv_probe.raw [build/meters/rv_music.raw ...]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import meter_frames as MF

FS = 48000
PPD, TOP = 155 / 36.0, 155.0          # 36 dB over the 155 px track, 0 dB at the top


def frame_peaks(y, period):
    a = np.abs(y); hop = int(round(period * FS)); n = len(a) // hop
    return np.arange(1, n + 1) * period, 20 * np.log10(np.maximum(a[:n * hop].reshape(n, hop).max(1), 1e-9))


def simulate(tf, p, rule, rate=26.7):
    bar = np.empty(len(p)); H, tH, prev, fall = -200.0, 0.0, -200.0, True
    for i, (t, v) in enumerate(zip(tf, p)):
        line = H - rate * (t - tH)
        if rule == "fall-mode" and fall and v > line: H, tH = v, t
        elif rule == "above-bar" and v > prev: H, tH = v, t
        elif rule == "above-hold" and (v > H or line < -100): H, tH = v, t
        elif rule == "classic" and v >= line: H, tH = v, t
        line = H - rate * (t - tH)
        b = max(v, line); fall = line >= v; bar[i] = b; prev = b
    return bar


if __name__ == "__main__":
    for path in sys.argv[1:]:
        t, bars, out = MF.load(path, "vox")
        L = bars["L"].astype(float) - 2
        m = (t > 0.3) & (L > 3) & (L < 143)
        for period in (1 / 30, 1 / 60, 0.02, 0.04):
            tf, p = frame_peaks(out[:, 0], period)
            for rule in ("fall-mode", "above-bar", "above-hold", "classic"):
                best = None
                for delay in np.arange(0.0, 0.08, 0.005):
                    sim = np.clip(TOP + PPD * simulate(tf, p, rule), 0, 155)
                    s = np.interp(t - delay, tf, sim)
                    e = np.sqrt(np.mean((s - L)[m] ** 2))
                    if best is None or e < best[0]: best = (e, delay)
                print("%s  frame %.1f ms  %-10s rms %.1f px (delay %.0f ms)" % (os.path.basename(path), period * 1000, rule, best[0], best[1] * 1000))


def onepole_on_change(tf, p, tau=0.325, quant=0.0):
    """One-pole (linear amplitude, instant rise) that only steps when the frame peak differs from the last one; the
    step then uses all the time since the previous step."""
    out = np.empty(len(p)); env, last, tl = 0.0, None, 0.0
    for i, (t, v) in enumerate(zip(tf, p)):
        q = np.round(v / quant) * quant if quant > 0 else v
        if last is None or q != last:
            a = 10 ** (q / 20) if q > -120 else 0.0
            env = a if a >= env else a + (env - a) * np.exp(-(t - tl) / tau)
            last, tl = q, t
        out[i] = 20 * np.log10(max(env, 1e-9))
    return out


def onepole(tf, p, tau=0.325):
    out = np.empty(len(p)); env, tl = 0.0, 0.0
    for i, (t, v) in enumerate(zip(tf, p)):
        a = 10 ** (v / 20) if v > -120 else 0.0
        env = a if a >= env else a + (env - a) * np.exp(-(t - tl) / tau); tl = t
        out[i] = 20 * np.log10(max(env, 1e-9))
    return out
