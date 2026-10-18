"""Settings-grid null test: WVox vs the reference over Compression x Gate x Gain x input level.

    python -I nullgrid_wvox.py [--quick] [--out ../docs/NULL_GRID.md] [key=value ...]

Signals: voice-like, white noise, tone steps (the hardest three from nulltest_wvox.py) plus a steady 1 kHz tone.
Residual = 20 log10(rms(WVox - reference) / rms(reference)) after the first 0.2 s, 48 kHz.
"""
import sys, os, itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import nulltest_wvox as N

FS = 48000
COMPS = [0, -6, -12, -18, -24, -30, -36]
GATES = [None, -60, -40, -20]
GAINS = [0, -12]
INPUTS = [-24, -12, 0]          # input trim applied to the test signal (dB)


def main():
    args = sys.argv[1:]
    quick = '--quick' in args
    out_md = args[args.index('--out') + 1] if '--out' in args else None
    tune = tuple(a for a in args if '=' in a and not a.startswith('--'))
    S = N.signals()
    sigs = {'voice-like': S['voice-like'], 'white noise': S['white noise -20 rms'], 'tone steps': S['tone steps -50/-10/-30'],
            'sine 1 kHz': R.tone(FS, 1000, 3.0, -20)}
    comps, gates, gains, inputs = (COMPS, GATES, GAINS, INPUTS) if not quick else ([0, -12, -36], [None, -40], [0], [-12, 0])
    rows = []
    for (sn, x0), comp, gate, gain, trim in itertools.product(sigs.items(), comps, gates, gains, inputs):
        x = (x0 * 10 ** (trim / 20)).astype(np.float32)
        ref = R.render(x, FS, comp=comp, gate=gate, gain=gain)[0]
        mine = N.ours(x, FS, comp, gate, gain, tune)[0]
        r = N.residual_db(mine, ref, int(0.2 * FS))
        rows.append((sn, comp, 'off' if gate is None else gate, gain, trim, r))
        print('%-12s comp %4d gate %4s gain %4d input %+4d  residual %7.1f' % rows[-1], flush=True)
    a = np.array([r[5] for r in rows])
    print('grid: %d cases, median %.1f dB, worst %.1f dB, %d below -60 dB' % (len(a), np.median(a), a.max(), (a < -60).sum()))
    if out_md:
        def table(key_idx, name, keys):
            lines = [f'| {name} | ' + ' | '.join(sn for sn in sigs) + ' |', '|---|' + '---:|' * len(sigs)]
            for k in keys:
                cells = []
                for sn in sigs:
                    v = [r[5] for r in rows if r[0] == sn and r[key_idx] == k]
                    cells.append('%.1f / %.1f' % (np.median(v), max(v)) if v else '')
                lines.append(f'| {k} | ' + ' | '.join(cells) + ' |')
            return '\n'.join(lines)
        with open(out_md, 'w', encoding='utf-8') as f:
            f.write('# WVox settings-grid null test\n\n')
            f.write('Every combination of Compression %s, Gate %s, Gain %s and an input trim of %s dB, for four signals (%d cases). '
                    % (comps, ['off' if g is None else g for g in gates], gains, inputs, len(rows)))
            f.write('Residual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after 0.2 s. Cells give **median / worst** in dB; lower is better, about -60 dB or less is a close match.\n\n')
            f.write('## By Compression\n\n' + table(1, 'Comp (dB)', comps) + '\n\n')
            f.write('## By Gate\n\n' + table(2, 'Gate (dB)', ['off' if g is None else g for g in gates]) + '\n\n')
            f.write('## By Gain\n\n' + table(3, 'Gain (dB)', gains) + '\n\n')
            f.write('## By input level\n\n' + table(4, 'Input trim (dB)', inputs) + '\n\n')
            f.write('Overall: median %.1f dB, worst %.1f dB, %d of %d cases below -60 dB.\n' % (np.median(a), a.max(), (a < -60).sum(), len(a)))


if __name__ == '__main__':
    main()
