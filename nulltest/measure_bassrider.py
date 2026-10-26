"""Phase 3 measurements of the reference Bass Rider (WaveShell VST3, the user's install), from its audio: the gain it
applies is the output over the input (pedalboard compensates the reported 42 ms latency: the output lines up). Every
experiment renders a fresh instance (the reference keeps note state between renders) at 48 kHz.

    python -I measure_bassrider.py static       steady-state gain against level, at three Targets (80 Hz sine)
    python -I measure_bassrider.py weighting    steady-state gain against frequency, and the low-shelf fit
    python -I measure_bassrider.py envelope     one / two / three tones, a square, noise: what level the rider reads
    python -I measure_bassrider.py gate         where riding starts, against Spill and frequency
    python -I measure_bassrider.py steps        the gain at legato steps up / down (look-ahead, smoothing, hold)
    python -I measure_bassrider.py noteoff      legato drops, Response Slow / Fast: when a note ends
    python -I measure_bassrider.py artifacts    the quietest re-attack that is a new note, against Artifacts
    python -I measure_bassrider.py release      how deep and how long a dip must be before a re-attack is a new note
    python -I measure_bassrider.py idle         the glide to the Idle value after a note ends
    python -I measure_bassrider.py live         the Live component's look-ahead
    python -I measure_bassrider.py stereo       what the stereo component listens to

Results print as tables; nothing rendered is kept. MEASUREMENTS.md ("WBass Rider: Phase 3") holds what they found.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal, optimize
import refhost as R

FS = 48000
NAME = "Bass Rider Mono"
HELD = -14 + 6.99 + 0.68            # the gain of a -6 dBFS note at 200 Hz (weighting -0.99 dB there), held


def render(x, name=NAME, **params):
    """x: mono float array (a stereo component gets (2, n)). A fresh instance per render."""
    R.fresh(name); p = R.load(name)
    for k, v in params.items(): setattr(p, k, v)
    x = np.asarray(x, np.float32)
    y = p(x if x.ndim == 2 else x[None], FS, buffer_size=512, reset=True)
    return y if x.ndim == 2 else y[0]


def rms_db(v): return 20 * np.log10(np.sqrt(np.mean(np.square(v, dtype=np.float64))) + 1e-20)


def steady_gain(f, db, secs=4.0, **params):
    t = np.arange(int(secs * FS)) / FS
    x = 10 ** (db / 20) * np.sin(2 * np.pi * f * t)
    y = render(x, **params)
    tail = slice(int((secs - 1.0) * FS), int((secs - 0.05) * FS))
    return rms_db(y[tail]) - rms_db(x[tail])


def seg(f, levels, durs):
    """a continuous sine (phase kept across the parts) at the given levels (dBFS peak) for the given durations"""
    parts = []; ph = 0.0
    for l, d in zip(levels, durs):
        n = int(d * FS); t = np.arange(n) / FS
        parts.append(10 ** (l / 20) * np.sin(2 * np.pi * f * t + ph)); ph += 2 * np.pi * f * n / FS
    return np.concatenate(parts)


def gtrace(x, y):
    """the applied gain (dB) sample by sample: a least-squares ratio over a sliding 2 ms window"""
    k = np.ones(int(0.002 * FS))
    return 20 * np.log10(np.maximum(np.convolve(x * y, k, 'same'), 1e-30) / np.maximum(np.convolve(x * x, k, 'same'), 1e-30))


def outcome(levels, durs, t_read, f=200.0, **kw):
    """the median gain over 0.1 s from t_read (s), for a sine made by seg"""
    x = seg(f, levels, durs); g = gtrace(x, render(x, **kw))
    return np.median(g[int(t_read * FS):int((t_read + 0.1) * FS)])


def law(level, f_weight_db=-0.99):
    """the gain a new note at this level (dBFS peak) gets at the defaults (200 Hz unless the weighting says otherwise)"""
    return float(np.clip(-14.0 - (level + f_weight_db) + 0.68, -12.0, 12.0))


def mark(gr, new, held=HELD, tol=0.2):
    return 'N' if abs(gr - new) < tol else ('h' if abs(gr - held) < tol else '%+.2f' % gr)


# ---------------------------------------------------------------------------------------------------------------------
def static():
    levels = np.arange(-60, 1, 3)
    for target in (-14.0, -24.0, -4.0):
        print('target %+.0f: level -> gain (dB), 80 Hz' % target)
        g = [steady_gain(80.0, float(l), target=target) for l in levels]
        for l, v in zip(levels, g): print('  %+4d  %+6.2f   (out %+6.1f)' % (l, v, l + v))


def rbj_lowshelf(fc, gdb, q):
    A = 10 ** (gdb / 40); w0 = 2 * np.pi * fc / FS; c = np.cos(w0); al = np.sin(w0) / (2 * q)
    b = np.array([A * ((A + 1) - (A - 1) * c + 2 * np.sqrt(A) * al), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - 2 * np.sqrt(A) * al)])
    a = np.array([(A + 1) + (A - 1) * c + 2 * np.sqrt(A) * al, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - 2 * np.sqrt(A) * al])
    return b / a[0], a / a[0]


def weighting():
    """the level read relative to the law at 80 Hz, low frequencies at 0 dBFS (off the +12 clamp), the rest at -8"""
    pts = {}
    for db, fs_ in ((0.0, (12, 15, 18, 20, 22, 25, 28, 30, 35, 40, 45, 50, 60, 70, 80)),
                    (-8.0, (90, 100, 110, 120, 140, 160, 180, 200, 250, 300, 400, 500, 600, 800, 1000, 1200, 1400, 1600, 1800, 1900))):
        for f in fs_:
            g = steady_gain(float(f), db)
            pts[f] = (-14 + 5.48 - db) - g
            print('  %6g Hz %+5.1f dBFS: gain %+6.2f, read %+6.2f dB re the 80 Hz law' % (f, db, g, pts[f]), flush=True)
    F = np.array([f for f in sorted(pts) if f < 1850]); W = np.array([pts[f] for f in F])
    resp = lambda p: 20 * np.log10(np.abs(signal.freqz(*rbj_lowshelf(p[0], p[1], p[2]), worN=2 * np.pi * F / FS)[1])) + p[3]
    s = optimize.least_squares(lambda p: resp(p) - W, [30, -30, 0.7, 4.8])
    print('RBJ low shelf fit: %.2f Hz, %.2f dB, Q %.4f, plateau %+.3f: rms %.3f dB, max %.3f' % (*s.x, np.sqrt(np.mean(s.fun ** 2)), np.abs(s.fun).max()))


def envelope():
    """steady signals of different crest: the reading of an analytic-envelope power follower (2 / 100 ms) predicts them"""
    secs = 4.0; t = np.arange(int(secs * FS)) / FS; tail = slice(int((secs - 1.0) * FS), int((secs - 0.05) * FS))
    a = 10 ** (-14 / 20)
    sq = signal.lfilter(*signal.butter(8, 1500, fs=FS), np.sign(np.sin(2 * np.pi * 300 * t)))
    n = signal.lfilter(*signal.butter(4, [300, 1500], 'band', fs=FS), np.random.default_rng(1).standard_normal(len(t)))
    sigs = {'1 kHz sine': a * np.sin(2 * np.pi * 1000 * t),
            '1 + 1.2 kHz': a * (np.sin(2 * np.pi * 1000 * t) + np.sin(2 * np.pi * 1200 * t)),
            '1 + 1.2 + 1.4 kHz': a * (np.sin(2 * np.pi * 1000 * t) + np.sin(2 * np.pi * 1200 * t) + np.sin(2 * np.pi * 1400 * t)),
            '300 Hz square (low-passed)': a * sq,
            'band noise, the sine rms': n * a / np.sqrt(2) / np.sqrt(np.mean(n ** 2))}
    ca, cr = np.exp(-1.0 / (0.002 * FS)), np.exp(-1.0 / (0.1 * FS))
    ref_g = ref_m = None
    for k, x in sigs.items():
        g = rms_db(render(x)[tail]) - rms_db(x[tail])
        p2 = (np.abs(signal.hilbert(x)) ** 2)[int((secs - 1.6) * FS):tail.stop].tolist(); s = 0.0; P = []
        for v in p2:
            c = ca if v > s else cr; s += (1 - c) * (v - s); P.append(s)
        m = 10 * np.log10(np.mean(P[int(0.6 * FS):]))
        if ref_g is None: ref_g, ref_m = g, m
        print('  %-28s gain %+6.2f: reads %+5.2f dB over the sine; the follower model %+5.2f' % (k, g, ref_g - g, m - ref_m), flush=True)


def gate():
    def ridden(f, db, **kw): return abs(steady_gain(f, db, secs=2.0, **kw)) > 0.05
    def start(f, **kw):
        lo, hi = -80.0, 0.0
        while hi - lo > 0.1:
            m = (lo + hi) / 2
            if ridden(f, m, **kw): hi = m
            else: lo = m
        return hi
    for spill in (-50.0, -40.0, -20.0, -3.0):
        print('  1 kHz, spill %+3.0f: riding starts at %+6.2f dBFS' % (spill, start(1000.0, spill=spill)), flush=True)
    for f in (30, 80, 150, 300, 500, 700, 1000, 1400, 1800):
        print('  %4d Hz: riding starts at %+6.2f dBFS' % (f, start(float(f))), flush=True)


def steps():
    f = 1000.0
    x = seg(f, [-30, -16], [1.0, 1.0]); g = gtrace(x, render(x))
    print('legato step -30 -> -16 dBFS (1 kHz) at 1 s; the gain (dB) at ms from the step:')
    print('  ' + ' '.join('%d:%+.2f' % (ms, g[int(1.0 * FS) + int(ms * FS / 1000)]) for ms in range(-46, 12, 2)))
    x = seg(f, [-16, -30], [1.0, 8.0]); g = gtrace(x, render(x))
    print('legato step -16 -> -30 at 1 s, then 8 s; the gain at s:')
    print('  ' + ' '.join('%.1f:%+.2f' % (s, np.median(g[int(s * FS):int(s * FS) + 960])) for s in (0.5, 1.0, 1.2, 2, 4, 6, 8, 8.9)))
    x = np.concatenate([seg(200.0, [-30], [2.0]), np.zeros(int(0.06 * FS)), seg(200.0, [-20], [2.0]), np.zeros(int(0.06 * FS)), seg(200.0, [-30], [2.0])])
    g = gtrace(x, render(x))
    print('200 Hz notes -30 / -20 / -30 with 60 ms gaps; the gain at s:')
    print('  ' + ' '.join('%.1f:%+.2f' % (s, np.median(g[int(s * FS):int(s * FS) + 960])) for s in (1.5, 3.5, 5.5)))


def noteoff():
    for resp in ('Slow ', 'Fast '):
        print('Response %s: -6 dBFS for 1 s, a legato drop of D dB; the gain at s after the drop' % resp.strip())
        for D in (12, 24, 36, 37, 38, 39, 40, 44):
            x = seg(1000.0, [-6, -6 - D], [1.0, 2.0]); g = gtrace(x, render(x, response=resp))
            print('  D %2d: ' % D + ' '.join('%.2f:%+.2f' % (s, np.median(g[int((1 + s) * FS):int((1 + s) * FS) + 480])) for s in (-0.1, 0.05, 0.1, 0.2, 0.3, 1.0, 1.9)), flush=True)
    print('a note at -30 dBFS, then -60 (30 dB down: under -38 dB? no): the gain at 0.5 / 1.4 s after: %+.2f %+.2f'
          % (outcome([-30, -60], [1.0, 1.5], 1.5, f=1000.0), outcome([-30, -60], [1.0, 1.5], 2.4, f=1000.0)))


def artifacts():
    print('-6 dBFS note, a drop to -40 for 0.3 s, a re-attack at L: N new note, h held (levels -8 .. -30 in 2 dB)')
    for art in (0.0, 25.0, 50.0, 75.0, 100.0):
        row = [mark(outcome([-6, -40, L], [0.6, 0.3, 0.6], 1.3, artifacts=art), law(L), tol=0.3) for L in range(-8, -31, -2)]
        print('  artifacts %3.0f: %s' % (art, ' '.join(row)), flush=True)


def release():
    print('-6 dBFS note, a legato dip X dB deep for 0.3 s, back up to -6.5: N new note, h held')
    for resp in ('Slow ', 'Fast '):
        for sens in (0.0, 50.0, 75.0, 100.0):
            row = ['%d:%s' % (X, mark(outcome([-6, -6 - X, -6.5], [0.6, 0.3, 0.6], 1.3, response=resp, sensitivity=sens), law(-6.5), tol=0.15))
                   for X in (1, 2, 3, 4, 5, 8, 10, 12, 13, 14)]
            print('  %s sens %3.0f: %s' % (resp.strip(), sens, ' '.join(row)), flush=True)
    print('the same with the dip T ms long')
    for resp, sens, X in (('Slow ', 50.0, 20), ('Fast ', 50.0, 6), ('Slow ', 100.0, 6)):
        row = ['%d:%s' % (T, mark(outcome([-6, -6 - X, -6.5], [0.6, T / 1000, 0.8], 1.1 + T / 1000, response=resp, sensitivity=sens), law(-6.5), tol=0.15))
               for T in (2, 5, 10, 20, 25, 30, 50)]
        print('  %s sens %3.0f, %d dB: %s' % (resp.strip(), sens, X, ' '.join(row)), flush=True)


def idle():
    print('a 1 kHz note at L for 1 s, then -60 dBFS: the gain at ms after the drop')
    for L, idle_db in ((-6.0, 0.5), (-20.0, 0.25), (-2.0, 0.98)):
        x = seg(1000.0, [L, -60.0], [1.0, 1.5]); g = gtrace(x, render(x, idle=idle_db))
        print('  L %+4.0f idle %.2f: ' % (L, idle_db) + ' '.join('%d:%+.2f' % (ms, np.median(g[int((1 + ms / 1000) * FS):int((1 + ms / 1000) * FS) + 240])) for ms in (-50, 0, 50, 100, 200, 300, 400, 1000)), flush=True)


def live():
    name = 'Bass Rider Live Mono'
    x = seg(1000.0, [-30, -16], [1.0, 1.0]); g = gtrace(x, render(x, name=name))
    print('%s: legato step -30 -> -16 at 1 s, the gain at ms from the step:' % name)
    print('  ' + ' '.join('%d:%+.2f' % (ms, g[int(1.0 * FS) + int(ms * FS / 1000)]) for ms in range(-10, 32, 2)))


def stereo():
    name = 'Bass Rider Stereo'
    xl = seg(1000.0, [-6.0], [2.0])
    for label, xr in (('R -30 dBFS', seg(1000.0, [-30.0], [2.0])), ('R = L', xl), ('R = -L', -xl), ('R a 1.2 kHz tone at -6', seg(1200.0, [-6.0], [2.0]))):
        y = render(np.stack([xl, xr]), name=name)
        print('  L -6 dBFS, %-24s gain L %+.2f, R %+.2f' % (label + ':', np.median(gtrace(xl, y[0])[FS:2 * FS - 2000]),
                                                         np.median(gtrace(xr, y[1])[FS:2 * FS - 2000]) if np.any(xr) else float('nan')), flush=True)


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'static'
    {'static': static, 'weighting': weighting, 'envelope': envelope, 'gate': gate, 'steps': steps, 'noteoff': noteoff,
     'artifacts': artifacts, 'release': release, 'idle': idle, 'live': live, 'stereo': stereo}[what]()
