"""A corpus of bass parts for matching WBass Rider's riding to the reference: real bass samples from FL Studio's own
packs (Instruments/Bass and Legacy/Instruments/Guitar/Bass: Ogg Vorbis in WAV, decoded by build/.../OggDecode.exe,
cached as 48 kHz float32 in reference/wbassrider/samples, git-ignored) played as bass lines (multisampled basses at
their own pitches, single samples resampled to the note), with rhythms, accents, ghost notes, rests, staccato and
legato, ringing overlaps, plus synthetic voices the packs lack (808 sub with a pitch drop, filtered saw, slow swells).
Every part is rebuilt from its seed; parts 0 .. 23 train, 24 .. 35 are held out.

    python -I bass_corpus.py                 decode the samples (once) and list the parts
    import bass_corpus as K; K.part(k)       part k: (float32 mono at 48 kHz, description)
"""
import sys, os, glob, json, struct, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import resample_poly

HERE = os.path.dirname(os.path.abspath(__file__))
FS = 48000
PACKS = r'C:\Program Files\Image-Line\FL Studio 2026\Data\Patches\Packs'
CACHE = os.path.join(HERE, '..', 'reference', 'wbassrider', 'samples')
DEC = os.path.join(HERE, '..', 'build', 'OggDecode_artefacts', 'Release', 'OggDecode.exe')
GROUPS = {'acoustic': r'Instruments\Bass\Acoustic', 'classic80': r'Instruments\Bass\Classic 80', 'ibanez': r'Instruments\Bass\Ibanez',
          'slap': r'Instruments\Bass\Slap', 'temptated': r'Instruments\Bass\Temptated',
          'bop': r'Legacy\Instruments\Guitar\Bass\FLStudio Bass 01', 'purple': r'Legacy\Instruments\Guitar\Bass\FLStudio Bass 02'}
NOTES = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}
TRAIN, HELD = range(0, 24), range(24, 36)


def _read(path):
    """a pack WAV: plain PCM, or an Ogg stream in the data chunk (format 0x674f)"""
    b = open(path, 'rb').read(); i = 12; fmt = data = None
    while i < len(b) - 8:
        cid, sz = b[i:i + 4], struct.unpack('<I', b[i + 4:i + 8])[0]
        if cid == b'fmt ': fmt = b[i + 8:i + 8 + sz]
        if cid == b'data': data = b[i + 8:i + 8 + sz]
        i += 8 + sz + (sz & 1)
    tag, ch, sr = struct.unpack('<HHI', fmt[:8])
    if tag == 1:
        bits = struct.unpack('<H', fmt[14:16])[0]
        x = np.frombuffer(data, '<i2' if bits == 16 else '<i4').astype(np.float64) / (2 ** (bits - 1))
        return x.reshape(-1, ch).mean(1), sr
    tmp = os.path.join(CACHE, '_tmp.ogg'); open(tmp, 'wb').write(data)
    out = os.path.join(CACHE, '_tmp.f32')
    sr2, ch2, n = subprocess.run([DEC, tmp, out], check=True, capture_output=True, text=True).stdout.split()
    x = np.fromfile(out, np.float32).astype(np.float64).reshape(-1, int(ch2)).mean(1)
    return x, int(float(sr2))


def _f0(x):
    """the pitch from the autocorrelation of 0.05 .. 0.35 s (25 .. 400 Hz)"""
    s = x[int(0.05 * FS):int(0.35 * FS)]; s = s - s.mean()
    if len(s) < 4000: s = x - x.mean()
    r = np.correlate(s, s, 'full')[len(s) - 1:]
    lo, hi = FS // 400, FS // 25
    k = lo + int(np.argmax(r[lo:hi]))
    # prefer the shortest lag with nearly the same correlation (an octave error otherwise)
    for d in (2, 3):
        kk = int(round(k / d))
        if kk >= lo and r[kk] > 0.85 * r[k]: k = kk
    return FS / k


def decode():
    os.makedirs(CACHE, exist_ok=True)
    index = {}
    for g, rel in GROUPS.items():
        for f in sorted(glob.glob(os.path.join(PACKS, rel, '*.wav'))):
            x, sr = _read(f)
            if sr != FS:
                from math import gcd
                d = gcd(FS, sr); x = resample_poly(x, FS // d, sr // d)
            x = x / (np.max(np.abs(x)) + 1e-12)
            name = '%s_%02d' % (g, len([k for k in index if k.startswith(g + '_')]))
            x.astype(np.float32).tofile(os.path.join(CACHE, name + '.f32'))
            stem = os.path.basename(f)
            midi = None
            for nn in sorted(NOTES, key=len, reverse=True):
                j = stem.rfind('_' + nn) if '_' + nn in stem else stem.rfind(' ' + nn)
                if j >= 0 and stem[j + 1 + len(nn):j + 2 + len(nn)].isdigit():
                    midi = 12 * (int(stem[j + 1 + len(nn)]) + 1) + NOTES[nn]; break
            index[name] = dict(file=stem, secs=len(x) / FS, f0=_f0(x), midi=midi)
            print('%-14s %-40s %.2f s  f0 %6.1f Hz  note %s' % (name, stem, len(x) / FS, index[name]['f0'], midi), flush=True)
    for t in ('_tmp.ogg', '_tmp.f32'):
        if os.path.exists(os.path.join(CACHE, t)): os.remove(os.path.join(CACHE, t))
    json.dump(index, open(os.path.join(CACHE, 'index.json'), 'w'), indent=1)


_IDX = None
def _index():
    global _IDX
    if _IDX is None: _IDX = json.load(open(os.path.join(CACHE, 'index.json')))
    return _IDX


def _sample(name): return np.fromfile(os.path.join(CACHE, name + '.f32'), np.float32).astype(np.float64)


def _hz(midi): return 440.0 * 2 ** ((midi - 69) / 12.0)


def _note_from_bank(group, midi):
    """a note from a multisampled bank (nearest sample, resampled the rest of the way) or a single sample resampled"""
    idx = _index()
    names = [k for k in idx if k.startswith(group + '_')]
    best = min(names, key=lambda k: abs(12 * np.log2(_hz(midi) / idx[k]['f0'])))
    x = _sample(best); ratio = _hz(midi) / idx[best]['f0']
    if abs(ratio - 1) > 0.003:
        t = np.arange(0, len(x) - 1, ratio); i = t.astype(int); fr = t - i
        x = x[i] * (1 - fr) + x[i + 1] * fr
    return x


def _synth(kind, midi, dur, rng):
    n = int(dur * FS) + int(0.6 * FS); t = np.arange(n) / FS; f = _hz(midi)
    if kind == '808':
        fi = f * (1 + 1.5 * np.exp(-t / 0.03)); ph = 2 * np.pi * np.cumsum(fi) / FS
        return np.tanh(1.5 * np.sin(ph)) * np.exp(-t / rng.uniform(0.4, 1.5))
    if kind == 'saw':
        ph = (f * t) % 1.0; x = 2 * ph - 1
        fc = 200 + 2500 * np.exp(-t / rng.uniform(0.05, 0.3)); y = np.zeros(n); s = 0.0
        for i in range(n):
            a = np.exp(-2 * np.pi * fc[i] / FS); s = a * s + (1 - a) * x[i]; y[i] = s
        return y * (1 - np.exp(-t / 0.003)) * np.exp(-t / rng.uniform(0.6, 3.0))
    # swell: a sustained tone whose level drifts slowly (a few dB over seconds)
    lev = np.cumsum(rng.standard_normal(n)) / np.sqrt(FS) * rng.uniform(3, 8)
    return np.sin(2 * np.pi * f * t) * 10 ** (lev / 20) * (1 - np.exp(-t / 0.01))


VOICES = ['bop', 'purple', 'acoustic', 'classic80', 'ibanez', 'slap', 'temptated', '808', 'saw', 'swell']


def part(k, secs=12.0):
    rng = np.random.default_rng(1000 + k)
    voice = VOICES[k % len(VOICES)]
    bpm = rng.uniform(75, 150); step = 60.0 / bpm / rng.choice([2, 4])   # eighths or sixteenths
    root = int(rng.integers(28, 41))                                       # E1 .. E2
    scale = [0, 2, 3, 5, 7, 8, 10, 12, -2, -5]
    style = rng.choice(['staccato', 'legato', 'ring', 'mixed'])
    n = int(secs * FS); out = np.zeros(n + 2 * FS); t = 0.15
    desc = dict(voice=voice, bpm=round(float(bpm), 1), style=str(style))
    while t < secs - 0.3:
        if rng.random() < 0.12: t += step * int(rng.integers(1, 4)); continue          # a rest
        length = step * int(rng.choice([1, 1, 2, 2, 3, 4, 6, 8]))
        midi = root + int(rng.choice(scale)) + (12 if rng.random() < 0.08 else 0)
        vel = rng.normal(0, 3.0) + (4 if rng.random() < 0.15 else 0) - (14 if rng.random() < 0.1 else 0)   # accents, ghosts
        if voice in ('808', 'saw', 'swell'): x = _synth(voice, midi, length, rng)
        else: x = _note_from_bank(voice, midi)
        st = style if style != 'mixed' else rng.choice(['staccato', 'legato', 'ring'])
        keep = {'staccato': 0.5 * length, 'legato': length, 'ring': length * rng.uniform(1.2, 2.5)}[st]
        i0 = int(t * FS)
        m = min(len(x), int(keep * FS) + int(0.015 * FS), len(out) - i0)
        env = np.ones(m); fade = int(0.012 * FS); env[-fade:] = np.linspace(1, 0, fade) if m > fade else env[-fade:]
        out[i0:i0 + m] += x[:m] * env * 10 ** (vel / 20)
        t += length
    y = out[:n]
    y *= 10 ** (rng.uniform(-18, -6) / 20) / (np.max(np.abs(y)) + 1e-12)
    desc['peak_dbfs'] = round(float(20 * np.log10(np.max(np.abs(y)))), 1)
    return y.astype(np.float32), desc


if __name__ == '__main__':
    if not os.path.exists(os.path.join(CACHE, 'index.json')): decode()
    for k in list(TRAIN) + list(HELD):
        x, d = part(k)
        print('part %2d %-5s %s  rms %.1f dBFS' % (k, 'train' if k in TRAIN else 'held', json.dumps(d), 10 * np.log10(np.mean(x.astype(float) ** 2) + 1e-20)), flush=True)
