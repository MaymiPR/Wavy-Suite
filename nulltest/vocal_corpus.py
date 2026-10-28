"""A corpus of vocal parts for matching WVox Rider's riding to the reference: the sung and spoken phrases of FL Studio's
own packs (Vocals, Legacy/Vocals: PCM or Ogg Vorbis in WAV, decoded by build/.../OggDecode.exe, cached as 48 kHz
float32 in reference/wvoxrider/samples, git-ignored), strung into 20 s parts: phrases at levels that wander (a singer
moving off the microphone, a louder chorus), gaps from a breath to a pause, sometimes a quiet room or bleed under
them. Every part is rebuilt from its seed; parts 0 .. 23 train, 24 .. 35 are held out.

    python -I vocal_corpus.py                 decode the samples (once) and list the parts
    import vocal_corpus as V; V.part(k)       part k: (float32 mono at 48 kHz, description)
"""
import sys, os, glob, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import resample_poly
import bass_corpus as B

HERE = os.path.dirname(os.path.abspath(__file__))
FS = 48000
CACHE = os.path.join(HERE, '..', 'reference', 'wvoxrider', 'samples')
GROUPS = {'webb': 'Vocals', 'legacy': r'Legacy\Vocals'}
TRAIN, HELD = range(0, 24), range(24, 36)
SECS = 20.0


def decode():
    os.makedirs(CACHE, exist_ok=True)
    B.CACHE = CACHE                                         # (its reader writes its temporary files there)
    index = {}
    for g, rel in GROUPS.items():
        for f in sorted(glob.glob(os.path.join(B.PACKS, rel, '*.wav'))):
            try:
                x, sr = B._read(f)
            except Exception as e:
                print('skip', f, e); continue
            if sr != FS:
                from math import gcd
                d = gcd(FS, sr); x = resample_poly(x, FS // d, sr // d)
            # trim silence at the ends (60 dB under the peak), normalise the peak to 0 dBFS
            a = np.abs(x); thr = a.max() * 1e-3
            nz = np.where(a > thr)[0]
            if len(nz) == 0 or a.max() == 0: continue
            x = x[max(0, nz[0] - 240):nz[-1] + 240] / a.max()
            if len(x) < 0.15 * FS: continue
            name = '%s_%03d' % (g, len(index))
            x.astype(np.float32).tofile(os.path.join(CACHE, name + '.f32'))
            index[name] = dict(file=os.path.relpath(f, B.PACKS), secs=len(x) / FS)
    json.dump(index, open(os.path.join(CACHE, 'index.json'), 'w'), indent=1)
    print('decoded %d phrases, %.0f s' % (len(index), sum(v['secs'] for v in index.values())))


_IDX = None


def _index():
    global _IDX
    if _IDX is None: _IDX = json.load(open(os.path.join(CACHE, 'index.json')))
    return _IDX


def phrase(name):
    return np.fromfile(os.path.join(CACHE, name + '.f32'), np.float32).astype(np.float64)


def part(k):
    """part k (seeded): phrases, levels, gaps, an optional floor"""
    rng = np.random.default_rng(7000 + k)
    names = sorted(_index())
    x = np.zeros(int(SECS * FS))
    base = rng.uniform(-24, -6)                      # the part's level (phrase peaks, dBFS)
    drift = rng.uniform(4, 14)                       # how far it wanders
    t = rng.uniform(0.1, 0.8)
    events = []
    while t < SECS - 0.5:
        nm = names[int(rng.integers(len(names)))]
        p = phrase(nm)
        lev = base + drift * np.sin(2 * np.pi * t / rng.uniform(6, 14) + rng.uniform(0, 6)) + rng.normal(0, 2.5)
        g = 10 ** (lev / 20)
        s = int(t * FS); e = min(len(x), s + len(p))
        x[s:e] += g * p[:e - s]
        events.append((round(t, 3), nm, round(float(lev), 1)))
        gap = float(rng.choice([rng.uniform(0.02, 0.15), rng.uniform(0.15, 0.6), rng.uniform(0.6, 2.5)], p=[0.35, 0.4, 0.25]))
        t += len(p) / FS + gap
    floor = None
    if rng.random() < 0.4:                           # a room / bleed under the voice
        floor = float(rng.uniform(-70, -45))
        n = rng.standard_normal(len(x))
        from scipy.signal import lfilter
        n = lfilter([1.0], [1.0, -0.97], n); n /= np.sqrt(np.mean(n ** 2))
        x += 10 ** (floor / 20) * n
    pk = np.abs(x).max()
    if pk > 10 ** (-1 / 20): x *= 10 ** (-1 / 20) / pk     # (overlapping phrases: kept under -1 dBFS)
    return x.astype(np.float32), dict(k=k, base=round(float(base), 1), drift=round(float(drift), 1), floor=floor, events=events)


if __name__ == '__main__':
    if not os.path.exists(os.path.join(CACHE, 'index.json')): decode()
    for k in list(TRAIN) + list(HELD):
        x, d = part(k)
        print('part %2d: base %+5.1f drift %4.1f floor %s, %d phrases, peak %+.1f dBFS' % (k, d['base'], d['drift'], d['floor'], len(d['events']), 20 * np.log10(np.abs(x).max())))
