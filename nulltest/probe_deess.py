"""Phase 4: probes of the main de-esser on the reference (every colour clear, Noise off), to pin its structure before
fitting: the split (a loud high tone holds the de-esser down while a quiet noise probe measures the chain's response,
whose ratio to the response without the tone is 1 - (1 - g) H(f) for the high band H), the static curve and the
detector (tones at levels and frequencies), and its timing. Writes deess_probe.npz.

    python -I probe_deess.py [split] [static] [timing]
"""
import os
import sys
import numpy as np
from scipy.signal import csd, welch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cv_ref as C
FS = 48000
OUT = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'deess_probe.npz')
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)


def ref(x, **kw):
    return C.render(np.asarray(x, np.float32), FS, {C.NOISE: 0}, C.host(**dict(CLEAR, **kw))).astype(float)


def cplx(x, y, nfft=4096):
    f, pxy = csd(x, y, FS, nperseg=nfft)
    _, pxx = welch(x, FS, nperseg=nfft)
    return f, pxy / np.maximum(pxx, 1e-30)


def split():
    rng = np.random.default_rng(3)
    n = FS * 6
    probe = 10 ** (-50 / 20) * rng.standard_normal(n)
    t = np.arange(n) / FS
    seg = slice(FS, None)
    y0 = ref(probe)
    f, h0 = cplx(probe[seg], y0[0, seg])
    res = {'f': f, 'h0': h0}
    for fd in (12000, 16000):
        for pk in (-12, -6, 0):
            x = probe + 10 ** (pk / 20) * np.sin(2 * np.pi * fd * t)
            y = ref(x)
            _, h = cplx(probe[seg], y[0, seg])
            r = h / h0
            res['r_%d_%d' % (fd, -pk)] = r
            print('drive %d Hz %+d dBFS: ratio at 1k %.2f dB, 3k %.2f, 4k %.2f, 5k %.2f, 6k %.2f, 8k %.2f, 10k %.2f, 14k %.2f' % (
                (fd, pk) + tuple(20 * np.log10(np.abs(r[np.argmin(np.abs(f - q))])) for q in (1000, 3000, 4000, 5000, 6000, 8000, 10000, 14000))), flush=True)
    return res


def static():
    """steady reduction of single tones (the tone's level out over the clear chain's linear gain at that frequency)"""
    rng = np.random.default_rng(4)
    res = {}
    n = FS * 2
    t = np.arange(n) / FS
    seg = slice(FS, None)
    lv = np.arange(-30, 7, 3)
    for f0 in (3000, 4000, 5000, 6000, 7000, 8000, 10000, 12000, 16000):
        lin = None
        row = []
        for pk in lv:
            x = 10 ** (pk / 20) * np.sin(2 * np.pi * f0 * t)
            y = ref(x)
            # the tone's complex amplitude in and out
            e = np.exp(-2j * np.pi * f0 * t[seg])
            a = 2 * np.mean(y[0, seg] * e) / (2 * np.mean(x[seg] * e))
            row.append(a)
        row = np.array(row)
        g = 20 * np.log10(np.abs(row / row[0]))
        res['tone_%d' % f0] = row
        print('%5d Hz:' % f0, ' '.join('%+.1f' % v for v in g), flush=True)
    res['levels'] = lv
    return res


def timing():
    """a 10 kHz burst from -30 to 0 dBFS and back: the gain's envelope at 0.1 ms"""
    n = FS * 3
    t = np.arange(n) / FS
    res = {}
    for f0, hi in ((10000, 0), (10000, -6), (6000, 0)):
        env = np.where((t > 1.0) & (t < 1.5), 10 ** (hi / 20), 10 ** (-30 / 20))
        x = env * np.sin(2 * np.pi * f0 * t)
        y = ref(x)
        res['burst_%d_%d' % (f0, -hi)] = y[0]
        res['burstx_%d_%d' % (f0, -hi)] = x
    return res


def amp(y, f0, seg):
    t = np.arange(len(y)) / FS
    e = np.exp(-2j * np.pi * f0 * t[seg])
    return 2 * np.mean(y[seg] * e)


def adaptive():
    """a 16 kHz tone (its band gain g = its own gain) at -3 dBFS peak with a low tone at each level: the 16 kHz tone's
    gain as the low tone grows"""
    n = FS * 2
    t = np.arange(n) / FS
    seg = slice(FS, None)
    res = {}
    lv = np.arange(-36, 7, 3)
    for fl in (100, 300, 1000, 2000, 3000):
        row = []
        for pk in lv:
            x = 10 ** (-3 / 20) * np.sin(2 * np.pi * 16000 * t) + 10 ** (pk / 20) * np.sin(2 * np.pi * fl * t)
            y = ref(x)[0]
            row.append(abs(amp(y, 16000, seg)) / 10 ** (-3 / 20))
        row = np.array(row)
        res['ad_%d' % fl] = row
        print('%5d Hz:' % fl, ' '.join('%+.1f' % v for v in 20 * np.log10(row / row[0] * 0 + row)), flush=True)
    res['ad_levels'] = lv
    return res


def detector():
    """the held level of tones from 5 to 20 kHz at 0 dBFS (the gain of the tone, its band's share known), and of pairs
    of high tones (peak against power)"""
    n = FS * 2
    t = np.arange(n) / FS
    seg = slice(FS, None)
    res = {}
    fr = np.arange(5000, 21001, 500)
    g = []
    for f0 in fr:
        x = np.sin(2 * np.pi * f0 * t)
        g.append(amp(ref(x)[0], f0, seg) / amp(x, f0, seg))
    res['det_f'] = fr; res['det_g'] = np.array(g)
    print('0 dBFS tones:', ' '.join('%d:%.2f' % (a, 20 * np.log10(abs(b))) for a, b in zip(fr, g)), flush=True)
    for pair in ((14000, 16000), (15000, 16000), (12000, 18000)):
        x = 0.5 * np.sin(2 * np.pi * pair[0] * t) + 0.5 * np.sin(2 * np.pi * pair[1] * t)
        y = ref(x)[0]
        a = abs(amp(y, pair[1], seg)) / 0.5
        res['pair_%d_%d' % pair] = a
        print('pair', pair, 'gain %.2f dB' % (20 * np.log10(a)), flush=True)
    for pk in (-6, 0):
        # a 16 kHz square-ish (a 16 kHz tone with its level at half the period) is not possible; a noise band instead
        rng = np.random.default_rng(5)
        from scipy.signal import butter, sosfilt
        nb = sosfilt(butter(8, [14000, 18000], 'bandpass', fs=FS, output='sos'), rng.standard_normal(n))
        nb = nb / np.sqrt(np.mean(nb ** 2)) * 10 ** (pk / 20) / np.sqrt(2)          # its rms that of a pk sine
        y = ref(nb)[0]
        from scipy.signal import welch
        _, px = welch(nb[seg], FS, nperseg=4096); f, py = welch(y[seg], FS, nperseg=4096)
        m = (f > 15000) & (f < 17000)
        res['nb_%d' % -pk] = 10 * np.log10(np.sum(py[m]) / np.sum(px[m]))
        print('noise band 14-18k, rms of a %+d sine: gain %.2f dB' % (pk, res['nb_%d' % -pk]), flush=True)
    return res


if __name__ == '__main__':
    data = dict(np.load(OUT)) if os.path.exists(OUT) else {}
    for a in sys.argv[1:] or ['split', 'static', 'timing', 'adaptive', 'detector']:
        data.update(globals()[a]())
        np.savez(OUT, **data)
