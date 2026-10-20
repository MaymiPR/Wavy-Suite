"""WBassX identification, step 2: the second-order kernel of G in the flat-gain region (Ratio 2, -50 dBFS per tone,
gain at its cap), from two-tone tests: K2(f1, f2) at f1 + f2 and f1 - f2, plus Decay dependence."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "maxxbass")


def two_tone(f1, f2, a, secs=2.0, **kw):
    n = int(secs * FS); t = np.arange(n) / FS
    x = a * (np.cos(2 * np.pi * f1 * t) + np.cos(2 * np.pi * f2 * t))
    y = R.render_mb(x.astype(np.float32), FS, harmonics_gain=0.0, monitor=1, **kw)[0].astype(np.float64)
    m = int(FS * 1.0); seg = y[-m:]; tt = t[-m:]
    pick = lambda f: 2 * np.mean(seg * np.exp(-2j * np.pi * f * tt)) if f > 0 else np.mean(seg)
    return {"sum": pick(f1 + f2) / (a * a), "diff": pick(abs(f1 - f2)) / (a * a), "h1": pick(f1) / a, "h2": pick(f2) / a,
            "2f1": pick(2 * f1) / (a * a), "2f2": pick(2 * f2) / (a * a)}


if __name__ == "__main__":
    F = float(sys.argv[1]) if len(sys.argv) > 1 else 80.0
    dec = float(sys.argv[2]) if len(sys.argv) > 2 else -15.0
    hp = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    a = 10 ** (-50 / 20)
    grid = [15, 20, 25, 30, 35, 40, 50, 60, 70, 80, 100, 120, 150]   # whole-Hz, 1 s windows: exact bins
    if F != 80: grid = sorted(set(int(round(g * F / 80)) for g in grid if g * F / 80 >= 8))
    rows = []
    for i, f1 in enumerate(grid):
        for f2 in grid[i + 1:]:
            r = two_tone(f1, f2, a, ratio=2.0, frequency=F, harmonics_decay=dec, highpass=hp)
            rows.append([f1, f2] + [v for k in ("sum", "diff", "h1", "h2", "2f1", "2f2") for v in (r[k].real, r[k].imag)])
        print(f1, flush=True)
    np.save(os.path.join(OUT, f"k2_F{F:g}_dec{dec:g}" + ("" if hp == 1 else f"_hp{hp}") + ".npy"), np.array(rows))
