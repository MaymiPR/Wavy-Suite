"""WBass identification step 2: the harmonic generator.
Linear paths (step 1): original = HP3(Freq) when Original Bass is out; generated = K * LP3(Freq/3) * HP3(Freq)^2 at low level.
Here: tones of amplitude A at f0, Intensity 0, Original Bass out. Each output harmonic h is divided by HP3(h f0)^2 (and the
original HP3 path is removed at h = 1); the generator's input is A * LP3(f0) (amplitude and phase known).
Writes measurements/rbass/nl.json: for each f0 and A the complex generator output per harmonic, phase-referred to the
generator's input."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, freqs as afreqs
import refhost as R

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "nl.json")
FSET = 160.0
NH = 12


def bw(order, kind, f, fc):
    b, a = butter(order, 1.0, btype=kind, analog=True)
    return afreqs(b, a, worN=np.atleast_1d(f) / fc)[1]


def comps(y, f0, nh, secs=1.0):
    per = FS / f0; n = int(round(int(secs * f0) * per))
    seg = np.asarray(y[-n:], np.float64); t = np.arange(n) / FS
    w = np.hanning(n); w /= w.sum() / 2
    return np.array([np.sum(seg * w * np.exp(-2j * np.pi * f0 * h * t)) for h in range(1, nh + 1)])


res = {"freq": FSET, "rows": []}
for f0 in (20.0, 30.0, 40.0):
    lp = bw(3, 'low', f0, FSET / 3)[0]
    for L in np.arange(-60, 0.1, 3.0):
        A = 10 ** (L / 20)
        x = R.tone(FS, f0, 2.0, L)
        y = R.render_rbass(x, FS, freq=FSET, intensity=0, hp=1)[0]
        Y = comps(y, f0, NH)
        Y[0] -= A * bw(3, 'high', f0, FSET)[0] * (-1j)           # remove the original path (sine: complex amp -j*A)
        hp2 = np.array([bw(3, 'high', h * f0, FSET)[0] ** 2 for h in range(1, NH + 1)])
        G = Y / hp2                                                # generator output, per harmonic
        a_in = A * lp                                              # generator input (complex amplitude of the sine)
        ph_in = np.angle(a_in * (-1j))
        Gref = G * np.exp(-1j * np.arange(1, NH + 1) * ph_in)      # phase referred to the input's phase (per harmonic)
        res["rows"].append({"f0": f0, "level": float(L), "a_in": float(abs(a_in)),
                            "re": Gref.real.tolist(), "im": Gref.imag.tolist()})
    print("f0", f0, "done", flush=True)
json.dump(res, open(OUT, "w"))
rows = res["rows"]
print("generator output |G_h| (dB) vs generator input amplitude, h = 1..6:")
for r in rows:
    if r["level"] % 12 == 0:
        g = np.abs(np.array(r["re"]) + 1j * np.array(r["im"]))
        print("f0 %4.0f  in %7.2f dB  " % (r["f0"], 20 * np.log10(r["a_in"])), " ".join("%7.1f" % (20 * np.log10(v + 1e-15)) for v in g[:6]))
