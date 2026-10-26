"""Probe: an Analog mode's transfer function at a low level, the reference against WDelayRender: a multitone of
integer-Hz sines (random phases, each at the given level; Mix 0: the coloured dry signal), measured over whole
seconds. Saves renders/hd_analog_tf<mode>.npz (f, H_ref, H_ours).

    python -I probe_hd_analog_tf.py mode [level]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wdelay as N

F = np.unique(np.round(np.geomspace(2, 22000, 90)).astype(int))

if __name__ == "__main__":
    fs = 48000; mode = int(sys.argv[1]); lev = float(sys.argv[2]) if len(sys.argv) > 2 else 0.001
    rng = np.random.default_rng(7)
    t = np.arange(int(6.0 * fs)) / fs
    x = sum(lev * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)) for f in F).astype(np.float32)
    x2 = np.stack([x, x])
    k = dict(analog="Mod%d" % mode, feedback=0, mix=0, preroll=1.0)
    r = N.theirs(x2, fs, **k)[0]; o = N.ours(x2, fs, **k)[0]
    s = slice(2 * fs, 6 * fs)
    X, R, O = (np.fft.rfft(v[s])[F * 4] for v in (x, r, o))
    Hr, Ho = R / X, O / X
    np.savez(os.path.join(N.TMP, 'hd_analog_tf%d.npz' % mode), f=F, Hr=Hr, Ho=Ho, lev=lev)
    for f, a, b in zip(F[::4], Hr[::4], Ho[::4]):
        print('%6d Hz  ref %7.3f dB %8.2f deg  ours %7.3f dB %8.2f deg  |diff| %6.1f dB' % (f, 20 * np.log10(abs(a)), np.degrees(np.angle(a)),
              20 * np.log10(abs(b)), np.degrees(np.angle(b)), 20 * np.log10(abs(a - b) / abs(a))))
