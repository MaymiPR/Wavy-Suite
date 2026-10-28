"""Reference renders for fitting WVox Rider's riding (Phase 4): the vocal corpus (vocal_corpus.py) and a set of
probes (steps, gaps, glides, syllable trains), each at a list of settings, through the reference Vocal Rider (fresh
instance per render). Cached as npz in reference/wvoxrider/renders (git-ignored): x (input), y (output).

    python -I vr_ref.py corpus | probes      render what is missing
    import vr_ref as Q; Q.load(kind, key, setting)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import vocal_corpus as V
from measure_voxrider import seg, with_pilot, FS

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'reference', 'wvoxrider', 'renders')
# settings: name -> (component, host parameters)
SETTINGS = {
    'default': ('Vocal Rider Mono', {}),
    'fast': ('Vocal Rider Mono', dict(attack='Fast ')),
    'vs+6': ('Vocal Rider Mono', dict(vocal_sensitivity=6.0)),
    'vs-6': ('Vocal Rider Mono', dict(vocal_sensitivity=-6.0)),
    'tgt-30': ('Vocal Rider Mono', dict(target=-30.0)),
    'tgt-12': ('Vocal Rider Mono', dict(target=-12.0)),
    'range12': ('Vocal Rider Mono', dict(range_min=-12.0, range_max=12.0)),
    'range-3+9': ('Vocal Rider Mono', dict(range_min=-3.0, range_max=9.0, idle=4.0)),
    'fast12': ('Vocal Rider Mono', dict(attack='Fast ', range_min=-12.0, range_max=12.0, idle=-4.0)),
    'live-30': ('Vocal Rider Live Mono', dict(spill=-30.0)),
}


def probes():
    """synthetic probes (each with the -80 dBFS pilot): level steps up / down, gaps, a stop, syllable trains"""
    P = {}
    for a, b in ((-30, -18), (-18, -30), (-24, -21), (-21, -24), (-36, -12), (-12, -36), (-27, -15)):
        P['step%+d%+d' % (a, b)] = with_pilot(seg(1000.0, [a, b], [3.0, 4.0]))
    for gap in (0.08, 0.3, 0.6, 1.5):
        for after in (-30, -18):
            P['gap%.2f%+d' % (gap, after)] = with_pilot(seg(1000.0, [-27, None, after], [3.0, gap, 2.5]))
    for lev in (-36, -24, -12):
        P['stop%+d' % lev] = with_pilot(seg(1000.0, [lev, None], [3.0, 5.0]))
    for f in (150.0, 400.0, 3000.0, 8000.0):
        P['tone%g' % f] = with_pilot(seg(f, [-30, -18, None, -24], [2.0, 2.0, 1.0, 2.0]))
    rng = np.random.default_rng(11)
    from probe_voxrider import buzz
    for k in range(4):
        x = np.zeros(int(10 * FS)); s = int(0.3 * FS)
        while s < len(x) - FS:
            n = int(rng.uniform(0.12, 0.5) * FS); tt = np.arange(n) / FS
            a = 10 ** (rng.uniform(-36, -8) / 20)
            x[s:s + n] += a * buzz(tt, rng.uniform(110, 330)) * np.abs(np.sin(np.pi * tt / tt[-1])) ** 0.4
            s += n + int(rng.choice([rng.uniform(0.03, 0.15), rng.uniform(0.2, 0.8), rng.uniform(1.0, 2.0)]) * FS)
        P['syll%d' % k] = with_pilot(x)
    return P


def render(x, comp, params):
    R.fresh(comp); p = R.load(comp)
    for k, v in params.items(): setattr(p, k, v)
    return p(np.asarray(x, np.float32)[None], FS, buffer_size=512, reset=True)[0]


def path(kind, key, setting): return os.path.join(OUT, '%s_%s_%s.npz' % (kind, key, setting))


def load(kind, key, setting):
    d = np.load(path(kind, key, setting)); return d['x'], d['y']


def items(kind):
    if kind == 'corpus':
        for k in list(V.TRAIN) + list(V.HELD): yield str(k), V.part(k)[0]
    else:
        for k, x in probes().items(): yield k, x


if __name__ == '__main__':
    kind = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    n = 0
    for key, x in items(kind):
        for s, (comp, params) in SETTINGS.items():
            p = path(kind, key, s)
            if os.path.exists(p): continue
            np.savez(p, x=np.asarray(x, np.float32), y=render(x, comp, params).astype(np.float32)); n += 1
    print('rendered', n)
