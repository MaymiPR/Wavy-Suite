"""Null test of WComp's factory presets against the reference H-Comp with the same settings (the reference's own
presets): each preset on program-like signals, with Analog Off (the compressor alone) and with the preset's Analog
mode (modes 1-3 add random noise that no null can remove).

    python -I nulltest_wcomp_presets.py [--out docs/NULL_RESULTS_WCOMP_PRESETS.md]
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wcomp as N
import nulltest_wvox as NV
import refhost as R

HERE = os.path.dirname(os.path.abspath(__file__))
PRESETS = os.path.join(HERE, '..', 'plugins', 'WComp', 'WCompPresets.h')
NOTES = ["1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4",
         "1/4D", "1/2T", "1/2", "1/2D", "1B", "2B"]
BEATS = [1 / 24, 1 / 16, 3 / 32, 1 / 12, 1 / 8, 3 / 16, 1 / 6, 1 / 4, 3 / 8, 1 / 3, 1 / 2, 3 / 4, 2 / 3, 1, 1.5, 4 / 3, 2, 3, 4, 8]
SIGNALS = ('voice-like', 'drum loop', 'bass line', 'pink noise -20 rms')
DEFAULTS = dict(threshold=-18.0, ratio=3.0, punch=1.0, mix=100.0, output=0.0, attack=7.0, release=100.25, releaseNote=5.0,
                sync=2.0, bpm=120.0, limiter=0.0, analog=2.0)


def synced_ms(note, bpm):
    while True:
        ms = BEATS[note] * 60000 / bpm
        if note <= 2 or ms <= (3960 if note == 19 else 3400): return ms
        note = 18 if note == 19 else 17 if note == 18 else note - (2 if note % 3 == 1 else 1)


def presets():
    s = open(PRESETS, encoding='utf-8').read()
    out = []
    for m in re.finditer(r'\{ "(\w+)", "([^"]+)", \{(.*?)\} \},', s):
        v = dict(DEFAULTS); v.update((a, float(b)) for a, b in re.findall(r'"(\w+)", ([-\d.e]+)f', m.group(3)))
        out.append((m.group(1), m.group(2), v))
    return out


def main():
    out_md = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else None
    S = N.signals(48000)
    rows = []
    for group, name, v in presets():
        R.fresh('H-Comp Stereo')               # a new reference per preset: it keeps state from earlier settings
        sync = int(v['sync']); note = int(v['releaseNote'])
        bpm = v['bpm'] if sync == 0 else 120.0          # Host: no host tempo here, both fall back to 120
        rel = v['release'] if sync == 2 else synced_ms(note, bpm)
        lim = 'On' if v['limiter'] > 0.5 else 'Off'
        res = {}
        for analog in (0, int(v['analog'])):
            for sig in SIGNALS:
                x = np.stack([S[sig], S[sig]]).astype(np.float32)
                a = N.ours(x, 48000, v['threshold'], v['ratio'], v['attack'], rel, v['punch'], v['mix'], v['output'], lim, analog)
                b = R.render_hc(x, 48000, threshold=v['threshold'], ratio=v['ratio'], attack=v['attack'], release=v['release'],
                                punch=v['punch'], mix=v['mix'], output=v['output'], limiter=lim, analog=analog or 'Off',
                                sync=['BPM', 'Host', 'ms'][sync], releasebpm=NOTES[note])
                res[(analog, sig)] = NV.residual_db(a[0], b[0], int(0.5 * 48000))
        rows.append((group, name, v, res))
        print('%-22s off: %s | analog %d: %s' % (name, ' '.join('%6.1f' % res[(0, s_)] for s_ in SIGNALS), int(v['analog']),
                                                ' '.join('%6.1f' % res[(int(v['analog']), s_)] for s_ in SIGNALS)), flush=True)
    off = np.array([[r[3][(0, s_)] for s_ in SIGNALS] for r in rows])
    print('median with Analog Off: %.1f dB; per signal %s' % (np.median(off), ' '.join('%.1f' % m for m in np.median(off, 0))))
    if out_md:
        with open(out_md, 'w', encoding='utf-8') as f:
            f.write('# WComp factory presets: null test\n\nEach preset against the reference H-Comp with the same settings (48 kHz, residual after 0.5 s, '
                    'dB re the reference output). "Analog Off" shows the compressor alone; the second set uses the preset\'s Analog mode '
                    '(modes 1-3 add random noise, which no null can remove). Produced by `nulltest/nulltest_wcomp_presets.py`.\n\n')
            f.write('| preset | ' + ' | '.join('%s (Off)' % s_ for s_ in SIGNALS) + ' | Analog | ' + ' | '.join(SIGNALS) + ' |\n')
            f.write('|---|' + '---:|' * len(SIGNALS) + '---|' + '---:|' * len(SIGNALS) + '\n')
            for group, name, v, res in rows:
                f.write('| %s | %s | %d | %s |\n' % (name, ' | '.join('%.1f' % res[(0, s_)] for s_ in SIGNALS), int(v['analog']),
                                                     ' | '.join('%.1f' % res[(int(v['analog']), s_)] for s_ in SIGNALS)))
            f.write('\nMedian with Analog Off: %.1f dB.\n' % np.median(off))


if __name__ == '__main__':
    main()
