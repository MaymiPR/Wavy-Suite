"""Phase 3 measurements of the reference Vocal Rider (WaveShell VST3, the user's install), from its audio: no latency,
so the gain it applies is the output over the input, sample by sample. Every experiment renders a fresh instance at
48 kHz (pedalboard; the side-chain through tools/RefRender).

    python -I measure_voxrider.py <command>

Results print as tables; nothing rendered is kept. MEASUREMENTS.md ("WVox Rider: Phase 3") holds what they found.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal, optimize
import refhost as R

FS = 48000
NAME = "Vocal Rider Mono"
LIVE = "Vocal Rider Live Mono"


def render(x, name=NAME, **params):
    """x: mono float array (a stereo component gets (2, n)). A fresh instance per render; parameters by their host
    names (pedalboard), set before rendering"""
    R.fresh(name); p = R.load(name)
    for k, v in params.items(): setattr(p, k, v)
    x = np.asarray(x, np.float32)
    y = p(x if x.ndim == 2 else x[None], FS, buffer_size=512, reset=True)
    return y if x.ndim == 2 else y[0]


def rms_db(v): return 20 * np.log10(np.sqrt(np.mean(np.square(v, dtype=np.float64))) + 1e-20)


def seg(f, levels, durs):
    """a continuous sine (phase kept across the parts) at the given levels (dBFS rms; None = silence) for the given
    durations (s)"""
    parts = []; ph = 0.0
    for l, d in zip(levels, durs):
        n = int(d * FS); t = np.arange(n) / FS
        a = 0.0 if l is None else 10 ** (l / 20) * np.sqrt(2)
        parts.append(a * np.sin(2 * np.pi * f * t + ph)); ph += 2 * np.pi * f * n / FS
    return np.concatenate(parts)


def gtrace(x, y, win=0.002):
    """the applied gain (dB) sample by sample: a least-squares ratio over a sliding window"""
    k = np.ones(max(1, int(win * FS)))
    return 20 * np.log10(np.maximum(np.convolve(x * y, k, 'same'), 1e-30) / np.maximum(np.convolve(x * x, k, 'same'), 1e-30))


PILOT_DB = -80.0


def with_pilot(x, f=3001.0):
    """x plus a quiet pilot tone (not a harmonic of the probes) that exposes the gain in silences"""
    t = np.arange(len(x)) / FS
    return x + 10 ** (PILOT_DB / 20) * np.sqrt(2) * np.sin(2 * np.pi * f * t)


def at(g, t0, t1):
    return float(np.median(g[int(t0 * FS):int(t1 * FS)]))


# ---------------------------------------------------------------------------------------------------------------------
def explore():
    """does a parameter set from pedalboard take effect; the gain over time on a steady tone; level and Target"""
    R.fresh(NAME); p = R.load(NAME)
    print({k: q.string_value for k, q in p.parameters.items() if q.string_value})
    x = seg(1000.0, [-30.0], [8.0])
    for tgt in (-21.0, -30.0):
        y = render(x, target=tgt)
        g = gtrace(x, y)
        print('target %+.0f: gain at 0.1 0.3 0.5 1 2 4 6 7.5 s:' % tgt, ' '.join('%+.2f' % at(g, a, a + 0.05) for a in (0.1, 0.3, 0.5, 1, 2, 4, 6, 7.5)))


def static():
    """the settled gain (after 6 s) against level, at 1 kHz, Targets -21 / -30 / -12, Range +-12"""
    for tgt in (-21.0, -30.0, -12.0):
        row = []
        for lev in (-50, -42, -36, -30, -27, -24, -21, -18, -15, -12, -9, -6):
            x = seg(1000.0, [lev], [7.0])
            y = render(x, target=tgt, range_max=12.0, range_min=-12.0)
            row.append(at(gtrace(x, y), 6.0, 6.9))
        print('target %+5.0f:' % tgt, ' '.join('%+6.2f' % v for v in row))




def settle():
    """the gain over 30 s on steady 1 kHz tones (Target -21, Range +-12): does it settle by 6 s"""
    for lev in (-36, -30, -21, -12):
        x = seg(1000.0, [lev], [30.0])
        g = gtrace(x, render(x, range_max=12.0, range_min=-12.0))
        print('%+d dBFS:' % lev, ' '.join('%+.2f' % at(g, a, a + 0.05) for a in (1, 2, 4, 6, 10, 15, 20, 25, 29.5)))


def read_level(gain_db, target=-21.0, rmin=-12.0, c=0.738):
    """the level (dB) the ride read, from its gain, by the law gain = 1/2 gMin + c Target / level (linear)"""
    a = 10 ** (gain_db / 20) - 0.5 * 10 ** (rmin / 20)
    return target - 20 * np.log10(max(a, 1e-9) / c)


def weighting():
    """the level the ride reads against frequency (1 kHz = 0), steady sines at -21 dBFS rms, Target -21, Range +-12"""
    out = []
    for f in (30, 50, 70, 100, 150, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 5000, 6500, 8000, 10000, 13000, 16000, 20000):
        x = seg(float(f), [-21.0], [3.0])
        g = at(gtrace(x, render(x, range_max=12.0, range_min=-12.0), win=0.02), 2.0, 2.9)
        out.append((f, g, read_level(g) + 21.0))
    ref = [o for o in out if o[0] == 1000][0][2]
    for f, g, l in out: print('%6d Hz  gain %+6.2f  read %+6.2f dB (re 1 kHz %+6.2f)' % (f, g, l, l - ref))


def bias():
    """low levels: the read level against the true level (1 kHz, Target -42, Range +-12): the follower's floor"""
    for lev in (-30, -36, -42, -48, -54, -60, -70):
        x = seg(1000.0, [lev], [3.0])
        g = at(gtrace(x, render(x, target=-42.0, range_max=12.0, range_min=-12.0), win=0.02), 2.0, 2.9)
        print('%+d dBFS: gain %+6.2f read %+7.2f' % (lev, g, read_level(g, target=-42.0)))


def steps():
    """the gain after steps of a 1 kHz tone (-30 <-> -18 dBFS, and +-6 dB), Attack Slow / Fast (Target -21, Range
    +-12): times to 63 % of the change, and the trace"""
    for attack in ('Slow ', 'Fast '):   # (the reference's texts end in a space)
        for a, b in ((-30, -18), (-18, -30), (-24, -18), (-18, -24)):
            x = seg(1000.0, [a, b], [3.0, 4.0])
            g = gtrace(x, render(x, range_max=12.0, range_min=-12.0, attack=attack), win=0.001)
            g0, g1 = at(g, 2.8, 2.95), at(g, 6.5, 6.9)
            k0 = int(3.0 * FS)
            tr = g[k0:k0 + int(3.0 * FS)]
            frac = (tr - g0) / (g1 - g0)
            def t_of(p):
                i = np.argmax(frac >= p)
                return i / FS * 1000 if frac[i] >= p else float('nan')
            pts = [0.001, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0]
            print('%-4s %+d -> %+d: %+.2f -> %+.2f  t10 %.1f t63 %.1f t90 %.1f ms | ' % (attack, a, b, g0, g1, t_of(0.1), t_of(0.632), t_of(0.9))
                  + ' '.join('%+.2f' % tr[int(p * FS)] for p in pts))


def idle():
    """after the tone stops (silence with a -80 dBFS pilot): the hold, then the glide to the Idle value; Attack Slow /
    Fast; Idle 0 and -3 dB (Range +-6)"""
    for attack in ('Slow ', 'Fast '):
        for lev in (-30.0, -12.0):
            for idle_db in (0.0, -3.0):
                x = with_pilot(seg(1000.0, [lev, None], [3.0, 8.0]))
                g = gtrace(x, render(x, attack=attack, idle=idle_db), win=0.005)
                g0 = at(g, 2.8, 2.95)
                k0 = int(3.0 * FS); tr = g[k0:]
                pts = [0.01, 0.03, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0, 5.0, 7.5]
                print('%-4s %+3.0f idle %+.0f: %+.2f |' % (attack, lev, idle_db, g0) + ' '.join('%+.2f' % at(tr, p, p + 0.004) for p in pts))
    print('times (s):', '0.01 0.03 0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.8 1.0 1.5 2.0 3.0 5.0 7.5')


def onset():
    """the quietest steady 1 kHz tone that is ridden (not parked at Idle -3 -> -6.94 at Range +-12... read as 'R' when
    the gain is off the Idle value), against Vocal Sensitivity and Target"""
    for tgt in (-21.0, -36.0):
        for vs in (-12.0, -6.0, 0.0, 6.0, 12.0):
            row = []
            for lev in range(-80, -29, 2):
                x = seg(1000.0, [None, float(lev)], [0.5, 2.5])
                g = gtrace(x, render(x, target=tgt, vocal_sensitivity=vs, range_max=12.0, range_min=-12.0, idle=-3.0), win=0.02)
                row.append(at(g, 2.6, 2.95))
            idle_g = row[0]
            first = next((lev for lev, v in zip(range(-80, -29, 2), row) if abs(v - idle_g) > 0.05), None)
            print('target %+.0f vocal sens %+5.1f: idle %+.2f, ridden from %s dBFS | %s' % (tgt, vs, idle_g, first, ' '.join('%+.1f' % v for v in row[::2])))


def spill():
    """Live: the quietest ridden steady 1 kHz tone against Spill (Vocal Sensitivity +12, so the relative gate is low);
    and that Studio ignores it"""
    for name, spills in ((LIVE, (-48.0, -45.0, -40.0, -35.0, -30.0, -24.0, -18.0)), (NAME, (None,))):
        for sp in spills:
            row = []
            levels = list(range(-60, -9, 1))
            for lev in levels:
                x = seg(1000.0, [None, float(lev)], [0.5, 2.0])
                kw = dict(vocal_sensitivity=12.0, range_max=12.0, range_min=-12.0, idle=-3.0)
                if sp is not None: kw['spill'] = sp
                g = gtrace(x, render(x, name=name, **kw), win=0.02)
                row.append(at(g, 2.1, 2.45))
            first = next((lev for lev, v in zip(levels, row) if abs(v - row[0]) > 0.05), None)
            print('%-22s spill %s: ridden from %s dBFS' % (name, sp, first))


def stereo():
    """the Stereo component: what level it reads from L / R (1 kHz, -24 dBFS rms per channel, Target -21, Range +-12):
    both equal, L alone, R alone, anti-phase, L -24 with R -30; and that both channels get the same gain"""
    x = seg(1000.0, [-24.0], [3.0]); z = np.zeros_like(x)
    cases = {'L=R': (x, x), 'L only': (x, z), 'R only': (z, x), 'anti-phase': (x, -x), 'L, R -6 dB': (x, 0.5 * x)}
    for k, (l, r) in cases.items():
        y = render(np.stack([l, r]), name="Vocal Rider Stereo", range_max=12.0, range_min=-12.0)
        gl = at(gtrace(l, y[0], win=0.02), 2.0, 2.9) if np.any(l) else float('nan')
        gr = at(gtrace(r, y[1], win=0.02), 2.0, 2.9) if np.any(r) else float('nan')
        g = gl if np.isfinite(gl) else gr
        print('%-11s gain L %+6.2f R %+6.2f -> read %+6.2f dB' % (k, gl, gr, read_level(g) ))
    y = render(x, range_max=12.0, range_min=-12.0)
    print('mono, same tone: read %+6.2f' % read_level(at(gtrace(x, y, win=0.02), 2.0, 2.9)))


def law(level, target=-21.0, rmin=-6.0, rmax=6.0, c=0.738):
    """the static law found: gain = clamp(1/2 gMin + c Target / level, gMin, gMax), linear (1 kHz, above the floor)"""
    g = 0.5 * 10 ** (rmin / 20) + c * 10 ** ((target - level) / 20)
    return 20 * np.log10(np.clip(g, 10 ** (rmin / 20), 10 ** (rmax / 20)))


def ranges():
    """the law against the measurement at other ranges (1 kHz, Target -21), and Output"""
    for rmin, rmax in ((-6.0, 6.0), (-3.0, 9.0), (-12.0, 0.0), (-1.0, 1.0)):
        meas, pred = [], []
        for lev in (-36, -30, -27, -24, -21, -18, -15, -12, -6):
            x = seg(1000.0, [lev], [2.5])
            meas.append(at(gtrace(x, render(x, range_min=rmin, range_max=rmax), win=0.02), 2.0, 2.4)); pred.append(law(lev, rmin=rmin, rmax=rmax))
        print('range %+.0f..%+.0f  meas %s' % (rmin, rmax, ' '.join('%+6.2f' % v for v in meas)))
        print('              law  %s' % ' '.join('%+6.2f' % v for v in pred))
    x = seg(1000.0, [-21.0], [2.5])
    for out in (-6.0, 3.0):
        print('output %+.0f: gain %+.2f (at output 0: %+.2f)' % (out, at(gtrace(x, render(x, output=out), win=0.02), 2.0, 2.4), law(-21.0)))


def gaps():
    """a 1 kHz tone at -30 (gain +6 at Range +-6 ... here Range +-12: +6.75) with a gap of silence (pilot only) of
    0.1 / 0.3 / 0.6 / 1.0 s, then the tone again at -30 or -18: the gain during and after the gap, Slow / Fast"""
    for attack in ('Slow ', 'Fast '):
        for gap in (0.1, 0.3, 0.6, 1.0):
            for after in (-30.0, -18.0):
                x = with_pilot(seg(1000.0, [-30.0, None, after], [3.0, gap, 2.0]))
                g = gtrace(x, render(x, attack=attack, range_max=12.0, range_min=-12.0, idle=-3.0), win=0.005)
                t1 = 3.0 + gap
                pts = [2.9, 3.0 + gap * 0.5, t1 - 0.01, t1 + 0.02, t1 + 0.05, t1 + 0.1, t1 + 0.3, t1 + 1.0, t1 + 1.9]
                print('%-4s gap %.1f then %+.0f: %s' % (attack, gap, after, ' '.join('%+.2f' % at(g, p, p + 0.005) for p in pts)))
    print('at: before, mid-gap, end of gap, +20 / 50 / 100 / 300 ms, +1 s, +1.9 s after')


def glide():
    """the glide to Idle after the tone stops (Attack Fast: 50 ms hold): from several gains, at Range +-12 / +-6 and
    Idle 0 / +6, the gain every 100 ms"""
    for rmin, rmax, idle_db in ((-12.0, 12.0, 0.0), (-6.0, 6.0, 0.0), (-12.0, 12.0, 6.0)):
        for lev in (-40.0, -33.0, -27.0, -21.0, -15.0, -9.0):
            x = with_pilot(seg(1000.0, [lev, None], [2.5, 6.0]))
            g = gtrace(x, render(x, attack='Fast ', range_min=rmin, range_max=rmax, idle=idle_db), win=0.005)
            tr = [at(g, 2.5 + p, 2.5 + p + 0.004) for p in np.arange(0.0, 5.01, 0.1)]
            print('range %+3.0f..%+3.0f idle %+.0f lev %+3.0f:' % (rmin, rmax, idle_db, lev), ' '.join('%+.2f' % v for v in tr[::2]))


if __name__ == '__main__':
    globals()[sys.argv[1]]()
