"""WBass identification: second-order kernel of the generator from two-tone tests.
Original Bass out (hp=1), Intensity 0, Freq 80. Each pair (fa, fb) of integer-Hz tones at -40 dBFS each; the components at
fa+fb and fb-fa can only come from the generator's quadratic part (the original path is linear, the level control's ripple
only makes odd-order products at low level). Stores the complex sum/difference components, phase-referred to the input
tones (sin convention), divided by the known output high-pass HP3(Freq)^2.
Writes measurements/rbass/k2.json."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, freqs as afreqs
import refhost as R

FS = 48000
F = 80.0
LEVEL = float(os.environ.get("K2_LEVEL", "-60"))
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", os.environ.get("K2_OUT", "k2_%d.json" % -int(float(os.environ.get("K2_LEVEL", "-60")))))
GRID = [int(v) for v in os.environ.get("K2_GRID", "11,15,21,29,41,57,79,113").split(",")]


def bw(o, k, f, fc):
    b, a = butter(o, 1.0, btype=k, analog=True)
    return afreqs(b, a, worN=np.atleast_1d(np.asarray(f, float)) / fc)[1]


def comp(y, f):
    n = FS; seg = np.asarray(y[-n:], np.float64); t = (np.arange(n) + len(y) - n) / FS
    return 2 * np.mean(seg * np.exp(-2j * np.pi * f * t))     # cos-referenced complex amplitude


rows = []
A = 10 ** (LEVEL / 20)
for i, fa in enumerate(GRID):
    for fb in GRID[i:]:
        t = np.arange(int(3.0 * FS)) / FS
        x = (A * np.sin(2 * np.pi * fa * t) + (A * np.sin(2 * np.pi * fb * t) if fb != fa else 0)).astype(np.float32)
        y = R.render_rbass(x, FS, freq=F, intensity=0, hp=1)[0]
        # inputs a*sin = a*cos(.-90deg): complex amplitudes -j*A. Sum product of the kernel: K * (-jA)(-jA) = -K A^2.
        s = comp(y, fa + fb) / bw(3, 'high', fa + fb, F)[0] ** 2
        Ks = s / (-(A ** 2)) / (1 if fb != fa else 0.5)          # a pure square of one tone gives half the cross term
        row = {"fa": fa, "fb": fb, "sum_re": Ks.real, "sum_im": Ks.imag}
        if fb != fa:
            dd = comp(y, fb - fa) / bw(3, 'high', fb - fa, F)[0] ** 2
            Kd = dd / ((-1j * A) * np.conj(-1j * A))              # b * conj(a)
            row.update({"dif_re": Kd.real, "dif_im": Kd.imag})
        rows.append(row)
        print("%4d %4d  sum %7.2f dB %7.1f deg" % (fa, fb, 20 * np.log10(abs(Ks)), np.degrees(np.angle(Ks))) +
              ("   dif %7.2f dB %7.1f deg" % (20 * np.log10(abs(Kd)), np.degrees(np.angle(Kd))) if fb != fa else ""), flush=True)
json.dump({"freq": F, "level": LEVEL, "rows": rows}, open(OUT, "w"))
