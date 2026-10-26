"""Phase 3 measurements of the reference Bass Rider (WaveShell VST3, the user's install), from its audio: the gain it
applies is the output over the input with the 42 ms latency removed.

    python -I measure_bassrider.py static      the steady-state gain against level, at a few Targets (80 Hz sine)
    python -I measure_bassrider.py weighting [dB]   the steady-state gain against frequency: the energy weighting

Results print as tables; nothing rendered is kept.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

FS = 48000
NAME = "Bass Rider Mono"
LAT = 2016                                                     # 42 ms at 48 kHz (reported, and the measured lag)


def render(x, **params):
    """x: mono float array. A fresh instance per render (the reference keeps note state between renders)."""
    R.fresh(NAME); p = R.load(NAME)
    for k, v in params.items(): setattr(p, k, v)
    y = p(np.asarray(x, np.float32)[None], FS, buffer_size=512, reset=True)[0]
    return np.r_[y[LAT:], np.zeros(LAT, np.float32)]


def rms_db(v): return 20 * np.log10(np.sqrt(np.mean(np.square(v, dtype=np.float64))) + 1e-20)


def steady_gain(f, db, secs=4.0, **params):
    t = np.arange(int(secs * FS)) / FS
    x = 10 ** (db / 20) * np.sin(2 * np.pi * f * t)
    y = render(x, **params)
    tail = slice(int((secs - 1.0) * FS), int((secs - 0.05) * FS))
    return rms_db(y[tail]) - rms_db(x[tail])


def static():
    levels = np.arange(-60, 1, 3)
    for target in (-14.0, -24.0, -4.0):
        print('target %+.0f: level -> gain (dB), 80 Hz' % target)
        g = [steady_gain(80.0, float(l), target=target) for l in levels]
        for l, v in zip(levels, g): print('  %+4d  %+6.2f   (out %+6.1f)' % (l, v, l + v))


def weighting(db=-8.0):
    print('frequency -> gain (dB) at %+.0f dBFS, the defaults (Target -14); energy re 80 Hz' % db)
    fs_ = (20, 25, 30, 40, 50, 60, 70, 80, 100, 120, 150, 200, 300, 500, 700, 1000, 1200, 1500, 2000, 5000)
    g = [steady_gain(float(f), db) for f in fs_]
    g80 = g[fs_.index(80)]
    for f, v in zip(fs_, g): print('  %6d  %+6.2f   %+6.2f' % (f, v, g80 - v))


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'static'
    if what == 'weighting': weighting(float(sys.argv[2]) if len(sys.argv) > 2 else -8.0)
    else: static()
