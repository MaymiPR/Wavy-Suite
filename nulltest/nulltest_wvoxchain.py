"""Phase 4 nulls: the reference CLA Vocals (cv_ref) against our WVox Chain VST3 (pedalboard), the same settings and
input, aligned (both report their latency); the null is the difference's energy under the reference output's (dB).

    python -I nulltest_wvoxchain.py [linear | all]
"""
import os
import sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cv_ref as C
import vocal_corpus as V

RENDER = os.path.abspath(os.path.join(HERE, '..', 'build', 'plugins', 'WVoxChain', 'WVoxChainRender_artefacts', 'Release', 'WVoxChainRender.exe'))
FS = 48000
STAGES = ('bass', 'treble', 'compress', 'reverb', 'delay', 'pitch')
CLEAR = {s + '_color': 0 for s in STAGES}
KEYS = {'sensitivity': 'sensitivity', 'output': 'output', 'balance': 'balance'}
for s in STAGES: KEYS[s] = s; KEYS[s + '_color'] = s + 'Color'


def render_ours(x, kw, fs=FS):
    """our plug-in's processor (tools/WVoxChainRender); the reference's defaults where kw says nothing (colours 1)"""
    import subprocess, tempfile
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x])
    d = tempfile.mkdtemp(prefix='wvc_')
    fin, fout = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    x.T.astype(np.float32).tofile(fin)
    args = [RENDER, fin, fout, '2', str(fs), 'noise=0'] + ['%s=%g' % (KEYS[k], v) for k, v in kw.items()]
    subprocess.run(args, check=True, capture_output=True, timeout=600)
    return np.fromfile(fout, np.float32).reshape(-1, 2).T


def null(kw, x, values=None):
    y = C.render(x, FS, values or {C.NOISE: 0}, C.host(**kw))         # (the Noise off in both: it cannot null)
    z = render_ours(x, kw)
    return 10 * np.log10(np.sum((y - z) ** 2) / max(np.sum(y ** 2), 1e-30)), y, z


def linear_cases():
    cases = [('clear', dict(CLEAR))]
    for st in ('bass', 'treble'):
        for c in (1, 2, 3):
            for v in (-10, -7.5, -3, 0, 2.5, 6, 10):
                cases.append(('%s c%d %+g' % (st, c, v), dict(CLEAR, **{st + '_color': c, st: v})))
    cases.append(('bass c2 +4, treble c3 -6', dict(CLEAR, bass_color=2, bass=4, treble_color=3, treble=-6)))
    for c in (1, 2, 3):
        for v in (0, 5, -7.3):
            cases.append(('reverb c%d %+g' % (c, v), dict(CLEAR, reverb_color=c, reverb=v)))
    cases.append(('reverb c2, sens +5, out -3, bal +3', dict(CLEAR, reverb_color=2, sensitivity=5, output=-3, balance=3)))
    cases.append(('reverb c3, bass c1 +6, treble c2 +4', dict(CLEAR, reverb_color=3, bass_color=1, bass=6, treble_color=2, treble=4)))
    return cases


def main():
    x = V.part(30)[0][:FS * 12]; x = x * 10 ** (-16 / 20) / np.abs(x).max()    # (a held-out vocal, its peak under the dynamics' thresholds)
    print('input: peak %.1f dBFS, rms %.1f dBFS' % (20 * np.log10(np.abs(x).max()), 20 * np.log10(np.sqrt(np.mean(x ** 2)))))
    res = []
    for name, kw in linear_cases():
        n, y, z = null(kw, x)
        res.append(n)
        print('  %-38s null %6.1f dB' % (name, n), flush=True)
    print('median %.1f dB, worst %.1f dB' % (np.median(res), max(res)))


if __name__ == '__main__':
    main()
