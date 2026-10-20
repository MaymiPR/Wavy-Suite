"""WBassX identification, step 1: the small-signal transfer of the generated part G (MaxxBass monitor).
Impulse responses at very low level (G is linear there); fitted against products of bilinear Butterworth sections."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, scipy.signal as ss
import refhost as R
FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "maxxbass")


def gir(a, n=FS, **kw):
    x = np.zeros(2 * n, np.float32); x[n] = a
    return R.render_mb(x, FS, harmonics_gain=0.0, monitor=1, **kw)[0][n:].astype(np.float64) / a


if __name__ == "__main__":
    res = {}
    for kw in (dict(), dict(ratio=4.0), dict(frequency=32), dict(frequency=256), dict(highpass=0), dict(highpass=2), dict(harmonics_decay=-24.0), dict(response=10.0)):
        h1 = gir(1e-4, **kw); h2 = gir(3e-4, **kw); h3 = gir(1e-5, **kw)
        lin = 20 * np.log10(np.linalg.norm(h1 - h2) / np.linalg.norm(h1))
        lin3 = 20 * np.log10(np.linalg.norm(h1 - h3) / np.linalg.norm(h1))
        H = np.fft.rfft(h1, 1 << 18); fr = np.fft.rfftfreq(1 << 18, 1 / FS)
        k = "_".join(f"{a}{b:g}" for a, b in kw.items()) or "default"
        res[k] = {"lin_1e-4_vs_3e-4_db": round(lin, 1), "lin_1e-4_vs_1e-5_db": round(lin3, 1), "dc_sum": round(float(h1.sum()), 5),
                  "mag": {str(f): round(float(20 * np.log10(abs(np.interp(f, fr, H.real) + 1j * np.interp(f, fr, H.imag)))), 2) for f in (5, 10, 20, 40, 60, 80, 120, 160, 240, 320, 640, 1280)}}
        np.save(os.path.join(R.__file__.rsplit(os.sep, 1)[0], "renders", f"mb_gir_{k}.npy"), h1)
        print(k, res[k])
    json.dump(res, open(os.path.join(OUT, "g_small_signal.json"), "w"), indent=1)
