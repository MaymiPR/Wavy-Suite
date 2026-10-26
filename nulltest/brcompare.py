"""Compares brmodel (WBass Rider's riding model) with the reference Bass Rider, case by case: the reference's applied
gain (its output over its input, sample by sample) against the model's ride, and the output residual.

    python -I brcompare.py [case ...]      (no case: all)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import measure_bassrider as M
import brmodel as B

FS = M.FS
HOST_KEYS = {'artifacts', 'sensitivity', 'response', 'target', 'range_min', 'range_max', 'idle', 'spill'}


def bassline():
    return np.fromfile(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'reference', 'wbassrider', 'bassline.f32'), np.float32)


def synth_notes(seed=3, secs=8.0, legato=0.3):
    """a synthetic bass part: notes 40 .. 200 Hz, levels -30 .. -3 dBFS, decays, some legato, ghost notes and gaps"""
    rng = np.random.default_rng(seed)
    x = np.zeros(int(secs * FS)); t0 = 0.05
    while t0 < secs - 0.3:
        dur = rng.uniform(0.08, 0.6); f = rng.uniform(40, 200); a = 10 ** (rng.uniform(-30, -3) / 20)
        if rng.uniform() < 0.15: a *= 0.1                                   # a ghost note
        n = np.arange(int(dur * FS)); env = np.exp(-n / FS / rng.uniform(0.1, 0.8)) * np.minimum(1, n / (0.002 * FS))
        env *= np.minimum(1, (len(n) - n) / (0.004 * FS))
        s = int(t0 * FS); x[s:s + len(n)] += a * env * np.sin(2 * np.pi * f * n / FS + rng.uniform(0, 6.3))
        t0 += dur * (1 - legato * (rng.uniform() < 0.3)) + rng.uniform(0, 0.08)
    return x.astype(np.float32)


CASES = {
    'steps 1k': (lambda: M.seg(1000.0, [-30, -16, -30, -20], [1.0, 1.0, 1.0, 1.0]), {}),
    'gaps 200': (lambda: np.concatenate([M.seg(200.0, [-30], [1.0]), np.zeros(2880), M.seg(200.0, [-20], [1.0]), np.zeros(2880), M.seg(200.0, [-30], [1.0])]), {}),
    'two tones': (lambda: (M.seg(1000.0, [-14], [3.0]) + M.seg(1200.0, [-14], [3.0])), {}),
    'noteoff': (lambda: M.seg(1000.0, [-6, -46, -60], [1.0, 1.0, 1.0]), {}),
    'dip slow': (lambda: M.seg(200.0, [-6, -20, -6.5, -10, -6.2], [0.6, 0.3, 0.6, 0.3, 0.6]), {}),
    'dip fast': (lambda: M.seg(200.0, [-6, -12, -6.5, -9, -6.2], [0.6, 0.3, 0.6, 0.3, 0.6]), {'response': 'Fast '}),
    'synth': (lambda: synth_notes(), {}),
    'bassline': (lambda: bassline(), {}),
}


def compare(name, show=True):
    make, kw = CASES[name]
    x = make().astype(np.float32)
    y = M.render(x, **{k: v for k, v in kw.items() if k in HOST_KEYS})
    gref = M.gtrace(x, y)
    mk = {k: v for k, v in kw.items() if k in HOST_KEYS and k != 'spill'}
    if 'response' in mk: mk['response'] = mk['response'].strip()
    gm = B.ride(x, **mk)
    ym = x * 10 ** (gm / 20)
    # where the input is too quiet for a gain to be read, the trace is meaningless
    env = np.convolve(x * x, np.ones(96) / 96, 'same')
    ok = env > 10 ** (-60 / 10)
    ok[:int(0.05 * FS)] = False
    d = (gm - gref)[ok]
    res = 10 * np.log10(np.sum((ym - y)[int(0.05 * FS):] ** 2) / np.sum(y[int(0.05 * FS):] ** 2) + 1e-30)
    if show:
        print('%-10s gain error: median |d| %.3f dB, 90%% %.3f, max %.2f; output residual %.1f dB' % (name, np.median(np.abs(d)), np.percentile(np.abs(d), 90), np.abs(d).max(), res), flush=True)
    return x, gref, gm, ok, res


if __name__ == '__main__':
    for name in (sys.argv[1:] or CASES):
        compare(name)
