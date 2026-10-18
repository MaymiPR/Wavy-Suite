"""WBass identification step 1: complex low-level responses of the reference, split into the original-signal path and
the generated path (Intensity acts as a gain on the generated path), for several Freq settings.
Writes measurements/rbass/linear_paths.json."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "linear_paths.json")
freqs = np.unique(np.round(np.geomspace(15, 4000, 60), 1))
LEVEL = -60.0


def h_at(f, y, x):
    per = FS / f; n = int(round(int(0.4 * f) * per)) if f > 10 else FS
    n = max(n, int(round(per)))
    t = np.arange(n) / FS; w = np.hanning(n)
    e = np.exp(-2j * np.pi * f * t)
    return np.sum(y[-n:] * w * e) / np.sum(x[-n:] * w * e)


def response(fset, inten, hp):
    out = []
    for f in freqs:
        x = R.tone(FS, f, max(1.0, 12.0 / f), LEVEL)
        y = R.render_rbass(x, FS, freq=fset, intensity=inten, hp=hp)[0]
        out.append(h_at(f, y.astype(np.float64), x.astype(np.float64)))
    return np.array(out)


res = {"freqs": freqs.tolist(), "level_dbfs": LEVEL}
for fset in (40, 80, 160, 256):
    for hp in (0, 1):
        H0 = response(fset, 0, hp); H24 = response(fset, -24, hp); H12 = response(fset, 12, hp)
        g24 = 10 ** (-24 / 20); g12 = 10 ** (12 / 20)
        Hgen = (H0 - H24) / (1 - g24)
        Horig = H0 - Hgen
        pred12 = Horig + g12 * Hgen
        err = 20 * np.log10(np.abs(pred12 - H12) / np.abs(H12))
        key = f"freq{fset}_hp{hp}"
        res[key] = {"orig_re": Horig.real.tolist(), "orig_im": Horig.imag.tolist(), "gen_re": Hgen.real.tolist(), "gen_im": Hgen.imag.tolist(),
                    "check_int12_err_db_median": float(np.median(err)), "check_int12_err_db_max": float(np.max(err))}
        print(key, "intensity-+12 check: median %.1f dB, worst %.1f dB" % (np.median(err), np.max(err)), flush=True)
        for f in (20, 40, 80, 160, 320, 1000):
            i = int(np.argmin(np.abs(freqs - f)))
            print("   %5.0f Hz  orig %6.2f dB %7.1f deg | gen %6.2f dB %7.1f deg" % (freqs[i], 20 * np.log10(abs(Horig[i]) + 1e-12), np.degrees(np.angle(Horig[i])),
                                                                                   20 * np.log10(abs(Hgen[i]) + 1e-12), np.degrees(np.angle(Hgen[i]))))
json.dump(res, open(OUT, "w"))
