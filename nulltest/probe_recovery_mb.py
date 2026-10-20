"""WBassX identification, step 6: how the level control recovers in silence and attacks at an onset.
A loud 50 Hz burst (0.3 s, -6 dBFS), a silent gap, then a probe burst (0.15 s); the gain at the probe's onset is
extracted (extract_g3, constrained) for each gap, Ratio and Response.

    python -I probe_recovery_mb.py [F]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R, extract_g3 as E3

FS = 48000
F = float(sys.argv[1]) if len(sys.argv) > 1 else 80.0
E3.KIND = "prev"
out = {}


def burst(secs, db, f=50.0):
    t = np.arange(int(secs * FS)) / FS
    env = np.minimum(1, t / 0.002) * np.minimum(1, (secs - t) / 0.002)
    return 10 ** (db / 20) * env * np.sin(2 * np.pi * f * t)


for ratio in (1.0, 2.0, 4.0):
    for resp in (10.0, 20.0, 30.0):
        for gap in (0.01, 0.03, 0.1, 0.3, 1.0):
            x = np.concatenate([np.zeros(int(0.05 * FS)), burst(0.3, -6), np.zeros(int(gap * FS)), burst(0.15, -12), np.zeros(int(0.05 * FS))])
            G = R.render_mb(x.astype(np.float32), FS, harmonics_gain=0.0, monitor=1, frequency=F, ratio=ratio, response=resp)[0].astype(np.float64)
            g, err = E3.extract(x, G, F, 1, -15.0, outer=10, iters=300)
            s0 = int((0.05 + 0.3 + gap) * FS)
            # gain over the probe: the knot values at 0, 2, 5, 10, 20, 50, 100 ms after its onset (dB)
            pts = {ms: round(float(20 * np.log10(max(np.median(g[s0 + int(ms * 48) - 24: s0 + int(ms * 48) + 24]), 1e-4))), 2)
                   for ms in (2, 5, 10, 20, 40, 70, 120)}
            key = f"r{ratio:g}_resp{resp:g}_gap{gap:g}"
            out[key] = dict(err=round(err, 1), g_db=pts, end_of_loud=round(float(20 * np.log10(np.median(g[s0 - int(gap * FS) - 2400: s0 - int(gap * FS) - 480]))), 2))
            print(key, out[key], flush=True)
json.dump(out, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "maxxbass", f"recovery_F{F:g}.json"), "w"), indent=1)
