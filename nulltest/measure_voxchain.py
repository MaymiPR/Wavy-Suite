"""Phase 3 measurements of the reference CLA Vocals (Stereo component), through nulltest/cv_ref.py (pedalboard; the
host's faders and colours; the state's Noise slot). Each section writes its data to the git-ignored
reference/wvoxchain/meas/<section>.npz / .json and prints a summary (MEASUREMENTS.md, WVox Chain: Phase 3).

    python -I measure_voxchain.py base eq gains noise ...
"""
import json
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cv_ref as C

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'reference', 'wvoxchain', 'meas')
os.makedirs(OUT, exist_ok=True)
QUIET = {C.NOISE: 0}                                         # the generated noise off (measured on its own)
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)
BANDS = (30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 16000, 20000)


def noise(db, secs, seed=1):
    return (10 ** (db / 20) * np.random.default_rng(seed).standard_normal(int(FS * secs))).astype(np.float32)


def response(params, db=-40.0, secs=4.0, nfft=16384, chan=0):
    """the linear transfer function (Welch) of the chain at a level where it is linear (below -18 dBFS rms)"""
    x = noise(db, secs)
    y = C.render(x, FS, QUIET, params)
    f, m, ph = C.tf(x, y[chan], FS, nfft)
    return f, m, ph, y


def at(f, m, qs=BANDS):
    return [float(m[np.argmin(abs(f - q))]) for q in qs]


def save(name, **arrays):
    np.savez_compressed(os.path.join(OUT, name + '.npz'), **arrays)


def base():
    """every colour clear: the impulse response (linear, -40 dBFS), and the level dependence (noise and tones)"""
    p = C.host(**CLEAR)
    x = np.zeros(FS, np.float32); x[2000] = 0.01
    y = C.render(x, FS, QUIET, p)
    ir = y[0, 2000:2000 + 8192] / 0.01
    f, m, ph, _ = response(p)
    print('base: IR peak %.4f at %d, DC sum %+.3f dB; L = R %s' % (ir[np.argmax(abs(ir))], np.argmax(abs(ir)), 20 * np.log10(abs(ir.sum())), np.allclose(y[0], y[1])))
    print('base: response %s' % ' '.join('%d:%+.2f' % (q, v) for q, v in zip(BANDS, at(f, m))))
    lv = {}
    for db in (-30, -24, -18, -15, -12, -9, -6, -3):
        fx, mx, _, yy = response(p, db=db, secs=2.0)
        lv[db] = at(fx, mx)
        print('base @ %+d dBFS rms: %s' % (db, ' '.join('%+.2f' % v for v in lv[db])), flush=True)
    save('base', ir=ir, f=f, m=m, ph=ph)
    json.dump({'bands': BANDS, 'levels': lv}, open(os.path.join(OUT, 'base_levels.json'), 'w'), indent=1)


def eq():
    """Bass and Treble: each colour and fader position against every colour clear (linear, -40 dBFS)"""
    f, m0, ph0, _ = response(C.host(**CLEAR))
    res = {'f': f, 'base': m0}
    for stage in ('bass', 'treble'):
        for col in (1, 2, 3):
            for v in (-10, -5, -2.5, 0, 2.5, 5, 10):
                kw = dict(CLEAR); kw[stage + '_color'] = col; kw[stage] = v
                _, m, ph, _ = response(C.host(**kw))
                d = m - m0
                res['%s_c%d_%+g' % (stage, col, v)] = d
                print('%-6s colour %d fader %+5.1f: %s' % (stage, col, v, ' '.join('%+.2f' % q for q in at(f, d))), flush=True)
    save('eq', **res)


def gains():
    """Sensitivity, Output and Balance (every colour clear, linear level): the gain of each step"""
    f, m0, _, y0 = response(C.host(**CLEAR), secs=2.0)
    r0 = [np.sqrt(np.mean(y0[c] ** 2)) for c in (0, 1)]
    for name, vals in (('sensitivity', (-10, -5, 5, 10)), ('output', (-10, -5, 5, 10)), ('balance', (-6, -3, 3, 6))):
        for v in vals:
            kw = dict(CLEAR); kw[name] = v
            _, _, _, y = response(C.host(**kw), secs=2.0)
            g = [20 * np.log10(np.sqrt(np.mean(y[c] ** 2)) / r0[c]) for c in (0, 1)]
            print('%-11s %+5.1f: L %+6.2f dB  R %+6.2f dB' % (name, v, g[0], g[1]), flush=True)


def noise_floor():
    """the Noise toggle: the output for silence with it on (defaults and every colour clear)"""
    from scipy.signal import welch
    x = np.zeros(FS * 4, np.float32)
    for tag, p in (('defaults', {}), ('clear', C.host(**CLEAR))):
        y = C.render(x, FS, {}, p)
        f, pxx = welch(y[0], FS, nperseg=8192)
        print('noise (%s): rms L %.1f dBFS R %.1f dBFS, L/R corr %.3f; spectrum %s' % (tag, 20 * np.log10(np.sqrt(np.mean(y[0] ** 2)) + 1e-12),
              20 * np.log10(np.sqrt(np.mean(y[1] ** 2)) + 1e-12), np.corrcoef(y[0], y[1])[0, 1] if y[0].std() > 0 else 0,
              ' '.join('%d:%.1f' % (q, 10 * np.log10(pxx[np.argmin(abs(f - q))] + 1e-30)) for q in BANDS)), flush=True)
        save('noise_' + tag, y=y.astype(np.float32))


def steps(levels, secs=0.6, f0=1000.0):
    """a 1 kHz tone stepping through levels (dBFS peak), secs each; returns the signal and each step's slice"""
    n = int(FS * secs); t = np.arange(n) / FS
    x = np.concatenate([10 ** (l / 20) * np.sin(2 * np.pi * f0 * t) for l in levels]).astype(np.float32)
    return x, [slice(i * n, (i + 1) * n) for i in range(len(levels))]


def compress():
    """Compress: the static curve (1 kHz tone, the last third of each 0.6 s step) per colour and fader against every
    colour clear; then the timing (a tone stepping -30 -> -6 -> -30 dBFS), per colour at fader 0"""
    levels = list(range(-45, 1, 3))
    x, sl = steps(levels)
    def curve(p):
        y = C.render(x, FS, QUIET, p)[0]
        return np.array([20 * np.log10(np.sqrt(np.mean(y[s.start + 2 * (s.stop - s.start) // 3:s.stop] ** 2)) /
                                        np.sqrt(np.mean(x[s.start + 2 * (s.stop - s.start) // 3:s.stop] ** 2))) for s in sl])
    g0 = curve(C.host(**CLEAR))
    res = {'levels': np.array(levels), 'clear': g0}
    print('compress clear: ' + ' '.join('%+d:%+.2f' % (l, g) for l, g in zip(levels, g0)), flush=True)
    for col in (1, 2, 3):
        for v in (-10, -5, 0, 5, 10):
            kw = dict(CLEAR); kw['compress_color'] = col; kw['compress'] = v
            g = curve(C.host(**kw)) - g0
            res['c%d_%+g' % (col, v)] = g
            print('compress colour %d fader %+3d: %s' % (col, v, ' '.join('%+.1f' % q for q in g)), flush=True)
    # timing: the output level against the input's, in 1 ms frames
    n = int(FS * 1.0); t = np.arange(3 * n) / FS
    env = np.concatenate([np.full(n, 10 ** (-30 / 20)), np.full(n, 10 ** (-6 / 20)), np.full(n, 10 ** (-30 / 20))])
    x = (env * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
    for col in (0, 1, 2, 3):
        kw = dict(CLEAR); kw['compress_color'] = col
        y = C.render(x, FS, QUIET, C.host(**kw))[0]
        fr = 48
        e = np.sqrt(np.mean((y[:len(y) // fr * fr].reshape(-1, fr)) ** 2, 1)) / np.sqrt(np.mean((x[:len(x) // fr * fr].reshape(-1, fr)) ** 2, 1))
        res['timing_c%d' % col] = 20 * np.log10(e + 1e-12)
        g = res['timing_c%d' % col]
        def t63(seg, start):
            d = seg - seg[-1]; k = np.argmax(np.abs(d) < 0.37 * abs(d[0]) + 1e-9); return k
        print('compress timing colour %d: gain before %.2f, at the step %.2f, settled %.2f (t63 %d ms); after the drop %.2f -> %.2f (t63 %d ms)' % (
            col, g[990], g[1001], g[1990], t63(g[1000:2000], 0), g[2001], g[2990], t63(g[2000:3000], 0)), flush=True)
    save('compress', **res)


def wet(stage, secs=4.0):
    """a wet stage (reverb, delay, pitch): its contribution (the output with the colour on less the output with it
    clear) to a -20 dBFS impulse, per colour and fader; the energy, the arrival, the decay, L / R"""
    x = np.zeros(int(FS * secs), np.float32); x[1000] = 0.1
    y0 = C.render(x, FS, QUIET, C.host(**CLEAR))
    res = {'dry': y0}
    for col in (1, 2, 3):
        for v in (-10, -5, 0, 5, 10):
            kw = dict(CLEAR); kw[stage + '_color'] = col; kw[stage] = v
            y = C.render(x, FS, QUIET, C.host(**kw))
            w = (y - y0)[:, 1000:] / 0.1
            res['c%d_%+g' % (col, v)] = w.astype(np.float32)
            e = np.sum(w ** 2, 1)
            first = int(np.argmax(np.max(np.abs(w), 0) > 0.01 * np.max(np.abs(w))))
            # Schroeder decay of L + R: the time from -5 to -35 dB, doubled (T60 by T30)
            sch = np.cumsum((w[0] ** 2 + w[1] ** 2)[::-1])[::-1]; sch = 10 * np.log10(sch / sch[0] + 1e-30)
            i5, i35 = np.argmax(sch < -5), np.argmax(sch < -35)
            corr = np.sum(w[0] * w[1]) / np.sqrt(e[0] * e[1] + 1e-30)
            print('%-6s colour %d fader %+3d: energy L %+6.1f R %+6.1f dB, first %5.1f ms, T60 %5.2f s, L/R corr %+.2f' % (
                stage, col, v, 10 * np.log10(e[0] + 1e-30), 10 * np.log10(e[1] + 1e-30), first / FS * 1000,
                2 * (i35 - i5) / FS if i35 > i5 else float('nan'), corr), flush=True)
    save('wet_' + stage, **res)


def deess():
    """the always-on high-frequency compression (every colour clear): the static gain of tones by frequency and level
    (against the linear gain at -45 dBFS), and its timing on a 6 kHz tone stepping -30 -> -6 -> -30 dBFS"""
    levels = list(range(-36, 1, 3))
    res = {'levels': np.array(levels)}
    p = C.host(**CLEAR)
    for f0 in (500, 1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 16000):
        x, sl = steps([-45] + levels, 0.5, f0)
        y = C.render(x, FS, QUIET, p)[0]
        g = np.array([20 * np.log10(np.sqrt(np.mean(y[s.start + 2 * (s.stop - s.start) // 3:s.stop] ** 2)) /
                                     np.sqrt(np.mean(x[s.start + 2 * (s.stop - s.start) // 3:s.stop] ** 2))) for s in sl])
        res['f%d' % f0] = g[1:] - g[0]
        print('deess %5d Hz: %s' % (f0, ' '.join('%+d:%+.1f' % (l, q) for l, q in zip(levels, g[1:] - g[0]))), flush=True)
    n = int(FS * 1.0); t = np.arange(3 * n) / FS
    env = np.concatenate([np.full(n, 10 ** (-30 / 20)), np.full(n, 10 ** (-6 / 20)), np.full(n, 10 ** (-30 / 20))])
    x = (env * np.sin(2 * np.pi * 6000 * t)).astype(np.float32)
    y = C.render(x, FS, QUIET, p)[0]
    fr = 48
    e = 20 * np.log10(np.sqrt(np.mean(y[:len(y) // fr * fr].reshape(-1, fr) ** 2, 1)) / np.sqrt(np.mean(x[:len(x) // fr * fr].reshape(-1, fr) ** 2, 1)) + 1e-12)
    res['timing'] = e
    print('deess timing 6 kHz: before %.2f, 1 / 2 / 5 / 10 / 20 / 50 / 100 / 500 ms after the rise %s; after the drop %s' % (
        e[990], ' '.join('%.2f' % e[1000 + k] for k in (1, 2, 5, 10, 20, 50, 100, 500)), ' '.join('%.2f' % e[2000 + k] for k in (1, 2, 5, 10, 20, 50, 100, 200, 500, 990))), flush=True)
    save('deess', **res)


if __name__ == '__main__':
    for a in sys.argv[1:]:
        {'base': base, 'eq': eq, 'gains': gains, 'noise': noise_floor, 'compress': compress, 'deess': deess,
         'reverb': lambda: wet('reverb'), 'delay': lambda: wet('delay'), 'pitch': lambda: wet('pitch', 2.0)}[a]()
