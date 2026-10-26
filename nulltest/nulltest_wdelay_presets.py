"""Null test of WDelay's factory presets against the reference H-Delay with the same preset values: the reference set
by exact normalised values (its own tapers: Delay's measured table, the rest linear or logarithmic), WDelay's renderer
by the values themselves. Both with the same pre-roll and timeline as nulltest_wdelay.py (Sync, then the time, in the
pre-roll; Feedback from the input's start). Mix as stored (the presets' own Mix, though loading one leaves Mix alone).

    python -I nulltest_wdelay_presets.py [--quick] [--out report.md]
"""
import sys, os, re, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import hdelay as H
import nulltest_wdelay as N

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')


def presets():
    """(group, name, {id: value}) from plugins/WDelay/WDelayPresets.h (the plug-in's own table)"""
    src = open(os.path.join(ROOT, 'plugins', 'WDelay', 'WDelayPresets.h'), encoding='utf-8').read()
    out = []
    for g, n, body in re.findall(r'\{ "([^"]+)", "([^"]+)", \{ (.*?) \} \},', src):
        out.append((g, n, {k: float(v) for k, v in re.findall(r'\{ "(\w+)", ([-0-9.e]+)f \}', body)}))
    return out


def delay_upper():
    src = open(os.path.join(ROOT, 'plugins', 'WDelay', 'WDelayProcessor.cpp'), encoding='utf-8').read()
    body = src[src.index('kDelayUpper[101] = {') + 20:]
    return [float(v) for v in body[:body.index('}')].replace('\n', ' ').split(',') if v.strip()]


UP = delay_upper()


def delay_norm(ms):
    ms = min(3500.0, max(1.0, ms))
    if ms <= 501.0: return (ms - 1.0) / 1000.0
    i = 0
    while i < 99 and UP[i + 1] < ms: i += 1
    return 0.5 + (i + (ms - UP[i]) / (UP[i + 1] - UP[i])) / 200.0


def lg(v, lo, hi): return math.log(v / lo) / math.log(hi / lo)


def render_pair(x, fs, v):
    sync = int(v['sync'])
    ref_args = ['preroll=3', 'Feedback:=0', 'Mix:=%.6f' % (v['mix'] / 100), 'Output:=%.6f' % ((v['output'] + 18) / 36),
                'HiPass:=%.6f' % lg(v['hipass'], 19, 20138.77), 'LoPass:=%.6f' % lg(min(v['lopass'], 20138.77), 19, 20138.77),
                'Depth:=%.6f' % (v['depth'] / 100), 'Rate:=%.8f' % lg(v['rate'], 0.1, 6000), 'BPM:=%.6f' % ((v['bpm'] - 30) / 270),
                H.arg('lofi', int(v['lofi'])), H.arg('pingpong', int(v['pingPong'])), H.arg('phase_l', int(v['phaseL'])),
                H.arg('phase_r', int(v['phaseR'])), H.arg('analog', int(v['analog'])),
                'at=-2.5:' + H.arg('sync', sync), 'at=-2.45:Delay Sec:=%.8f' % delay_norm(v['delay']),
                'at=-2.45:' + H.arg('note', int(v['delayNote'])), 'at=0:Feedback:=%.6f' % (v['feedback'] / 200)]
    r, _ = R.render_ref(x, fs, name=H.NAMES['Stereo'], bpm=120.0, extra=ref_args)
    kw = dict(feedback=0, mix=v['mix'], output=v['output'], hipass=v['hipass'] if v['hipass'] >= 19.5 else 'Off',
              lopass=v['lopass'] if v['lopass'] < 20100 else 'Off', depth=v['depth'], rate=v['rate'], bpm=v['bpm'],
              lofi=H.CHOICE['lofi'][1][int(v['lofi'])], pingpong=H.CHOICE['pingpong'][1][int(v['pingPong'])],
              phase_l=H.CHOICE['phase_l'][1][int(v['phaseL'])], phase_r=H.CHOICE['phase_r'][1][int(v['phaseR'])],
              analog=H.CHOICE['analog'][1][int(v['analog'])], preroll=3.0,
              changes=[(-2.5, 'sync', H.CHOICE['sync'][1][sync]), (-2.45, 'delay', v['delay']), (-2.45, 'note', H.NOTES[int(v['delayNote'])]),
                       (0.0, 'feedback', v['feedback'])])
    return N.ours(x, fs, **kw), r


def main():
    a = sys.argv[1:]
    quick = '--quick' in a
    out_md = a[a.index('--out') + 1] if '--out' in a else None
    fs = 48000
    S = N.signals(fs)
    lines = ['| group | preset | Analog | ' + ' | '.join(S) + ' |', '|---|---|---|' + '---|' * len(S)]
    P = presets()
    for g, name, v in (P[::8] if quick else P):
        row = []
        for sname, x in S.items():
            o, r = render_pair(np.stack([x, 0.8 * x]), fs, v)
            row.append('%.1f' % N.residual_db(o, r))
        lines.append('| %s | %s | %s | %s |' % (g, name, ['Off', '1', '2', '3', '4'][int(v['analog'])], ' | '.join(row)))
        print(lines[-1], flush=True)
    if out_md: open(out_md, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
