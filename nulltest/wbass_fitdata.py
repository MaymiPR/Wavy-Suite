"""Renders the RBass fitting set: the generated path alone, split off with two Intensity settings (Intensity is an exact
gain on the generated path), for tones, level steps, two-tones, noise and decaying notes.
    python -I wbass_fitdata.py [freq]        -> measurements/rbass/fitdata_<freq>.npz (git-ignored)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, sosfilt
import refhost as R

FS = 48000
F = float(sys.argv[1]) if len(sys.argv) > 1 else 80.0
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "fitdata_%d.npz" % int(F))
G24 = 10 ** (-24 / 20)


def tone(f, secs, db): return R.tone(FS, f, secs, db).astype(np.float64)


def signals():
    s = {}
    rng = np.random.default_rng(7)
    for f in (0.3, 0.6, 1.0, 1.5):
        for lvl in (-40, -20, -6):
            s["tone_%g_%d" % (f, lvl)] = tone(f * F, 1.2, lvl)
    s["step_50_10"] = np.concatenate([tone(0.6 * F, 0.6, -50), tone(0.6 * F, 1.0, -10)[:], tone(0.6 * F, 1.5, -50)])
    s["step_40_20"] = np.concatenate([tone(0.6 * F, 0.6, -40), tone(0.6 * F, 1.0, -20), tone(0.6 * F, 1.5, -40)])
    t = np.arange(int(1.5 * FS)) / FS
    s["twotone"] = 0.1 * np.sin(2 * np.pi * 0.41 * F * t) + 0.1 * np.sin(2 * np.pi * 0.89 * F * t)
    for lvl in (-40, -20):
        n = rng.standard_normal(int(2.0 * FS))
        n = sosfilt(butter(2, 4 * F, fs=FS, output='sos'), n)
        s["noise_%d" % lvl] = n / np.sqrt(np.mean(n ** 2)) * 10 ** (lvl / 20)
    env = np.exp(-t / 0.35)
    s["pluck"] = 0.7 * env * np.sin(2 * np.pi * 0.52 * F * t) + 0.2 * env * np.sin(2 * np.pi * 1.04 * F * t)
    return s


if __name__ == "__main__":
    data = {}
    for name, x in signals().items():
        x32 = x.astype(np.float32)
        y0 = R.render_rbass(x32, FS, freq=F, intensity=0, hp=1)[0].astype(np.float64)
        y1 = R.render_rbass(x32, FS, freq=F, intensity=-24, hp=1)[0].astype(np.float64)
        data["x_" + name] = x32
        data["gen_" + name] = (y0 - y1) / (1 - G24)
        data["orig_" + name] = (y1 - G24 * y0) / (1 - G24)
        print(name, "done", flush=True)
    np.savez_compressed(OUT, **data)
    print("wrote", OUT)
