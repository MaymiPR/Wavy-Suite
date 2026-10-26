"""Probe: an Analog mode's low-frequency response, steady sines one at a time at a low level (Mix 0: the coloured dry
signal), the reference against WDelayRender: gain and phase per frequency.

    python -I probe_hd_analog_lf.py mode [level]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wdelay as N

if __name__ == "__main__":
    fs = 48000; mode = int(sys.argv[1]); lev = float(sys.argv[2]) if len(sys.argv) > 2 else 0.003
    t = np.arange(int(3.0 * fs)) / fs
    for f in (5, 10, 15, 20, 30, 45, 70, 100, 150, 200, 300, 500, 1000, 3000, 10000):
        x = (lev * np.sin(2 * np.pi * f * t)).astype(np.float32); x2 = np.stack([x, x])
        k = dict(analog="Mod%d" % mode, feedback=0, mix=0, preroll=1.0)
        r = N.theirs(x2, fs, **k)[0]; o = N.ours(x2, fs, **k)[0]
        s = slice(int(1.5 * fs), int(3.0 * fs)); e = np.exp(-2j * np.pi * f * t[s])
        X = np.dot(x[s], e); R = np.dot(r[s], e); O = np.dot(o[s], e)
        print('%6g Hz  ref %7.3f dB %7.2f deg   ours %7.3f dB %7.2f deg   null %6.1f dB' % (f, 20 * np.log10(abs(R / X)), np.degrees(np.angle(R / X)),
              20 * np.log10(abs(O / X)), np.degrees(np.angle(O / X)), N.residual_db(o[None, s], r[None, s])), flush=True)
