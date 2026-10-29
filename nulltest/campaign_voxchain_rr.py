"""Phase 3, the campaign's parts that need tools/RefRender (a host tempo, the Mono/Stereo component, parameter changes
during playback): the delays at 40 .. 240 bpm, the Mono/Stereo component against the Stereo one across the colours,
and how each control's change glides. Writes reference/wvoxchain/meas/campaign/<section>.json and prints a summary.

    python -I campaign_voxchain_rr.py tempo monostereo changes
"""
import json
import os
import sys
import numpy as np
from concurrent.futures import ThreadPoolExecutor
from scipy.signal import find_peaks

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import refhost as R

OUT = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'campaign')
FS = 48000
STEREO, MONO = 'CLA Vocals Stereo@1510537014', 'CLA Vocals Mono/Stereo@1511767094'
NAMES = {'bass': 'Bass', 'treble': 'Treble', 'compress': 'Compress', 'reverb': 'Reverb', 'delay': 'Delay', 'pitch': 'Pitch'}


def settings(kw):
    """RefRender arguments (normalised values) for host settings in real units"""
    out = ['preroll=1']
    for k, v in kw.items():
        if k.endswith('_color'): out.append('%s Color:=%g' % (NAMES[k[:-6]], v / 3.0))
        elif k == 'balance': out.append('Balance:=%g' % ((v + 6.0) / 12.0))
        elif k in ('sensitivity', 'output'): out.append('%s:=%g' % (k.capitalize(), (v + 10.0) / 20.0))
        else: out.append('%s:=%g' % (NAMES[k], (v + 10.0) / 20.0))
    return out


CLEAR = {s + '_color': 0 for s in NAMES}


def rr(x, kw, name=STEREO, bpm=0.0, extra=()):
    y, _ = R.render_ref(x, FS, name, bpm=bpm, extra=settings(kw) + list(extra))
    return y


def tempo():
    x = np.zeros(FS * 4, np.float32); x[1000] = 0.1
    jobs = [(bpm, c) for bpm in range(40, 241, 10) for c in (0, 1, 2, 3)]
    def run(j):
        bpm, c = j
        kw = dict(CLEAR); kw['delay_color'] = c
        return j, rr(x, kw, bpm=bpm)
    with ThreadPoolExecutor(12) as ex: res = dict(ex.map(run, jobs))
    out = []
    print('== Tempo: the delays\' echoes (ms; L / R peak levels in dB) and the note lengths')
    for bpm in range(40, 241, 10):
        y0 = res[(bpm, 0)]
        for c in (1, 2, 3):
            w = (res[(bpm, c)] - y0)[:, 1000:] / 0.1
            env = np.abs(w).max(0)
            pk, _ = find_peaks(env, height=env.max() * 0.03, distance=int(0.04 * FS))
            first = int(np.argmax(env > 0.05 * env.max()))
            echoes = [(round(k / FS * 1000, 1), round(float(20 * np.log10(abs(w[0, k]) + 1e-9)), 1), round(float(20 * np.log10(abs(w[1, k]) + 1e-9)), 1)) for k in pk[:6]]
            out.append(dict(bpm=bpm, colour=c, first_ms=round(first / FS * 1000, 2), echoes=echoes))
            print('  %3d bpm c%d: 1/16 %.1f, 1/8 %.1f, 1/4 %.1f ms | first %.1f | %s' % (bpm, c, 15000 / bpm, 30000 / bpm, 60000 / bpm, first / FS * 1000,
                  '  '.join('%.1f (%+.0f/%+.0f)' % e for e in echoes)))
    json.dump(out, open(os.path.join(OUT, 'tempo.json'), 'w'))


def monostereo():
    r = np.random.default_rng(4)
    x = (0.03 * r.standard_normal(FS * 3)).astype(np.float32)
    configs = {'clear': CLEAR, 'defaults': {}}
    for s in NAMES:
        for c in (1, 2, 3): configs['%s %d' % (s, c)] = dict(CLEAR, **{s + '_color': c, s: 5.0})
    def run(item):
        name, kw = item
        ys = rr(np.stack([x, x]), kw, STEREO); ym = rr(x, kw, MONO)
        return name, ys, ym
    with ThreadPoolExecutor(12) as ex: res = list(ex.map(run, configs.items()))
    out = []
    print('== Mono/Stereo against Stereo fed the same signal on both inputs: the difference (dB under the output), per side')
    for name, ys, ym in sorted(res):
        d = [10 * np.log10(np.mean((ys[c] - ym[c]) ** 2) / max(np.mean(ys[c] ** 2), 1e-30) + 1e-30) for c in (0, 1)]
        lv = [20 * np.log10(np.sqrt(np.mean(ym[c] ** 2)) / max(np.sqrt(np.mean(ys[c] ** 2)), 1e-12)) for c in (0, 1)]
        out.append(dict(config=name, diff=[float(v) for v in d], level=[float(v) for v in lv]))
        print('  %-11s L %+7.1f dB  R %+7.1f dB   (level Mono/Stereo over Stereo: %+.2f / %+.2f dB)' % (name, d[0], d[1], lv[0], lv[1]))
    json.dump(out, open(os.path.join(OUT, 'monostereo.json'), 'w'))


def changes():
    """a control jumped half way through a steady tone (at= after 1.5 s): the output level in 1 ms frames round it"""
    t = np.arange(FS * 3) / FS
    x = (0.05 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
    xh = (0.05 * np.sin(2 * np.pi * 5000 * t)).astype(np.float32)
    jobs = [('sensitivity -10 -> +10', x, {}, 'Sensitivity:=0', 'Sensitivity:=1'),
            ('output -10 -> +10', x, {}, 'Output:=0', 'Output:=1'),
            ('balance 0 -> -6 (R)', x, {}, 'Balance:=0.5', 'Balance:=0'),
            ('compress -10 -> +10 (Push)', x, {'compress_color': 1}, 'Compress:=0', 'Compress:=1'),
            ('treble -10 -> +10 (Bite, 5 kHz)', xh, {'treble_color': 1}, 'Treble:=0', 'Treble:=1'),
            ('bass -10 -> +10 (Sub, 100 Hz)', None, {'bass_color': 1}, 'Bass:=0', 'Bass:=1'),
            ('delay -10 -> +10 (Slap)', x, {'delay_color': 1}, 'Delay:=0', 'Delay:=1'),
            ('bass colour Sub -> Lower (100 Hz fader +10)', None, {'bass': 10.0}, 'Bass Color:=0.333333', 'Bass Color:=0.666667'),
            ('compress colour Push -> Wall', x, {}, 'Compress Color:=0.333333', 'Compress Color:=1'),
            ('reverb -10 -> +10', x, {}, 'Reverb:=0', 'Reverb:=1'),
            ('reverb colour clear -> Tight', x, {}, 'Reverb Color:=0', 'Reverb Color:=0.333333')]
    x100 = (0.05 * np.sin(2 * np.pi * 100 * t)).astype(np.float32)
    def run(j):
        name, sig, kw, a, b = j
        sig = x100 if sig is None else sig
        base = dict(CLEAR); base.update(kw)
        if 'reverb' in name.lower() and 'colour' not in name: base['reverb_color'] = 1
        y = rr(sig, base, extra=[a, 'at=1.5:' + b])
        fr = FS // 1000
        e = 20 * np.log10(np.sqrt(np.mean(y[:, :len(sig) // fr * fr].reshape(2, -1, fr) ** 2, 2)) + 1e-12)
        return name, e
    with ThreadPoolExecutor(12) as ex: res = list(ex.map(run, jobs))
    out = []
    print('== Parameter changes at 1.5 s: the output level (dB, L) in 1 ms frames from 5 ms before; the time to 90 % of the step')
    for name, e in res:
        L = e[1] if '(R)' in name else e[0]; before, after = L[1490], L[2900]
        d = L[1500:2900] - before
        k90 = int(np.argmax(np.abs(d) >= 0.9 * abs(after - before))) if abs(after - before) > 0.2 else None
        out.append(dict(change=name, env=L[1480:1700].round(2).tolist(), before=float(before), after=float(after), t90_ms=k90))
        if '100 Hz' in name:                                       # (a 100 Hz tone: its level per 10 ms period)
            L10 = 20 * np.log10(np.sqrt(np.mean((10 ** (L[:len(L) // 10 * 10] / 20)).reshape(-1, 10) ** 2, 1)) + 1e-12)
            print('  %-44s per 10 ms: %s' % ('', ' '.join('%+.1f' % v for v in L10[147:160])))
        print('  %-44s %+.2f -> %+.2f dB, 90 %% in %s ms: %s' % (name, before, after, k90, ' '.join('%+.1f' % v for v in L[1495:1560:4])))
    json.dump(out, open(os.path.join(OUT, 'changes.json'), 'w'))


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for a in sys.argv[1:]: globals()[a]()
