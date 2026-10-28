"""WVox Rider's factory presets against the reference's own (build/ref_presets_VocalRider.json, written by
nulltest/read_ref_presets_vr.py): the reference with its preset array loaded into its state (wavesstate.apply_values,
so it takes the values exactly, hidden ones included), WVox Rider with its factory preset (WVoxRiderRender preset=k),
both Mono, on three held-out vocal parts; the output residual after the first 0.1 s. Writes
docs/NULL_RESULTS_WVOXRIDER_PRESETS.md.

    python -I nulltest_wvoxrider_presets.py
"""
import sys, os, json, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R, wavesstate as W, vocal_corpus as V
import nulltest_wvoxrider as N

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'build', 'ref_presets_VocalRider.json')
OUT = os.path.join(HERE, '..', 'docs', 'NULL_RESULTS_WVOXRIDER_PRESETS.md')
FS = 48000
COMP = 'Vocal Rider Mono'
PARTS = (24, 27, 33)


def names():
    src = open(os.path.join(HERE, '..', 'tools', 'make_presets.py'), encoding='utf-8').read()
    a = src.index('VOCALRIDER_NAMES = [') + len('VOCALRIDER_NAMES = ')
    return eval(src[a:src.index(']', a) + 1])


def ref(x, raw):
    R.fresh(COMP); p = R.load(COMP)
    W.apply_values(p, raw)
    return p(np.asarray(x, np.float32)[None], FS, buffer_size=512, reset=True)[0]


def ours(x, k):
    fi, fo = os.path.join(N.TMP, 'vpin%d.f32' % os.getpid()), os.path.join(N.TMP, 'vpout%d.f32' % os.getpid())
    np.asarray(x, np.float32).tofile(fi)
    subprocess.run([N.EXE, fi, fo, '1', str(FS), 'mode=1', 'preset=%d' % k], check=True, capture_output=True)
    return np.fromfile(fo, np.float32)


def residual(y, r):
    a = int(0.1 * FS)
    return 20 * np.log10(np.sqrt(np.mean((y[a:] - r[a:]) ** 2)) / np.sqrt(np.mean(r[a:] ** 2)) + 1e-30)


def main():
    data = json.load(open(SRC)); nm = names()
    assert len(data) == len(nm)
    mats = [(k, V.part(k)[0]) for k in PARTS]
    rows = []
    for k, (name, d) in enumerate(zip(nm, data)):
        res = [residual(ours(x, k), ref(x, d['raw'])) for _, x in mats]
        rows.append((name, res))
        print('%-14s %s' % (name, '  '.join('part %d %6.1f dB' % (p, v) for (p, _), v in zip(mats, res))), flush=True)
    allv = [v for _, r in rows for v in r]
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('# WVox Rider: the factory presets against the reference\'s own\n\n')
        f.write('Each of the reference\'s 13 presets loaded into its state (its stored values, hidden ones included), against\n'
                'WVox Rider\'s preset of the same settings (Wavy names), both Mono, on held-out vocal parts of the corpus\n'
                '(`nulltest/vocal_corpus.py`); the output residual after the first 0.1 s, dB (lower is better). Made by\n'
                '`nulltest/nulltest_wvoxrider_presets.py`. The Music Sensitivity a preset stores has no effect on either\n'
                'side here (no side-chain).\n\n')
        f.write('| preset | %s |\n|---|%s\n' % (' | '.join('part %d' % p for p in PARTS), '---|' * len(PARTS)))
        for name, r in rows: f.write('| %s | %s |\n' % (name, ' | '.join('%.1f' % v for v in r)))
        f.write('\nMedian %.1f dB, worst %.1f dB over %d renders.\n' % (np.median(allv), max(allv), len(allv)))
    print('median %.1f worst %.1f' % (np.median(allv), max(allv)))


if __name__ == '__main__':
    main()
