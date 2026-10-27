"""The reference Bass Rider's riding on the bass corpus (bass_corpus.py): its applied gain on every part at a set of
settings, through pedalboard (lined up with the input), kept at 0.5 ms (the 2 ms least-squares gain trace, every 24th
sample) in measurements/wbassrider/corpus/<part>_<setting>.npy (git-ignored).

    python -I br_corpus_ref.py [parts]      render the missing ones (parts: e.g. 0-35, default all)
    import br_corpus_ref as CR; CR.load(k, setting)  -> (gain at 48 kHz, interpolated from the 0.5 ms trace)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import bass_corpus as K

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'corpus')
DEC = 24
SETTINGS = {'default': {}, 'fast': {'response': 'Fast'}, 's0': {'sensitivity': 0.0}, 's100': {'sensitivity': 100.0},
            'fast25': {'response': 'Fast', 'sensitivity': 25.0}, 'fast100': {'response': 'Fast', 'sensitivity': 100.0},
            's75': {'sensitivity': 75.0}, 'art15': {'artifacts': 15.0}, 'art85': {'artifacts': 85.0}, 'tgt24': {'target': -24.0}}


def path(k, s): return os.path.join(OUT, '%02d_%s.npy' % (k, s))


def render(k, s):
    import measure_bassrider as M
    x, _ = K.part(k)
    st = dict(SETTINGS[s])
    if 'response' in st: st['response'] = st['response'] + ' '
    g = M.gtrace(x, M.render(x, **st))
    np.save(path(k, s), g[::DEC].astype(np.float32))


def load(k, s):
    g = np.load(path(k, s)).astype(np.float64)
    n = len(K.part(k)[0])
    return np.interp(np.arange(n), np.arange(len(g)) * DEC, g)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    parts = range(36)
    if len(sys.argv) > 1:
        a, b = sys.argv[1].split('-'); parts = range(int(a), int(b) + 1)
    for k in parts:
        for s in SETTINGS:
            if not os.path.exists(path(k, s)): render(k, s)
        print('part %d done' % k, flush=True)
