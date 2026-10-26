"""Null test: WBass Rider (its own processor, run offline by build/.../WBassRiderRender.exe) against the reference Bass
Rider (rendered through pedalboard) at 48 kHz, both lined up with the input (the reported latency removed), stereo, the
same input on both channels unless the case says otherwise. Reports per case the output residual 20 log10 (rms (ours - reference) / rms (reference)) after the first
0.1 s, and the applied gain's error (ours vs the reference's, sample by sample where the input is loud enough).
Numbers are reported as they come: no case is called a null unless the data shows it.

    python -I nulltest_wbassrider.py [--quick] [--out report.md]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import subprocess
import refhost as R
import measure_bassrider as M
import brcompare as C

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get('WBASSRIDER_RENDER', os.path.join(HERE, '..', 'build', 'plugins', 'WBassRider', 'WBassRiderRender_artefacts', 'Release', 'WBassRiderRender.exe'))
TMP = os.path.join(HERE, 'renders'); os.makedirs(TMP, exist_ok=True)
FS = 48000
KEYS = {'target': 'target', 'sensitivity': 'sensitivity', 'spill': 'spill', 'artifacts': 'artifacts', 'range_max': 'rangeMax',
        'range_min': 'rangeMin', 'output': 'output', 'idle': 'idle'}


def render_ours(x, **params):
    """x: (channels, n). The reference's host parameter names and units"""
    x = np.atleast_2d(np.asarray(x, np.float32)); ch, n = x.shape
    fi, fo = os.path.join(TMP, 'brin%d.f32' % os.getpid()), os.path.join(TMP, 'brout%d.f32' % os.getpid())
    x.T.astype(np.float32).tofile(fi)
    args = [EXE, fi, fo, str(ch), str(FS), 'mode=%d' % (0 if ch == 2 else 1)]
    for k, v in params.items():
        if k == 'response': args.append('response=%d' % (1 if v.strip() == 'Fast' else 0))
        else: args.append('%s=%g' % (KEYS[k], v))
    subprocess.run(args, check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, ch).T


def render_ref(x, **params):
    R.fresh('Bass Rider Stereo'); p = R.load('Bass Rider Stereo')
    for k, v in params.items(): setattr(p, k, v)
    return p(np.asarray(x, np.float32), FS, buffer_size=512, reset=True)


def cases(quick):
    t = np.arange(int(4 * FS)) / FS
    out = [('steady 80 Hz -20 dBFS', M.seg(80.0, [-20], [4.0]), {}),
           ('steps 1 kHz', M.seg(1000.0, [-30, -16, -30, -20], [1.0, 1.0, 1.0, 1.0]), {}),
           ('bass line', C.bassline(), {}),
           ('synthetic part 6', C.synth_notes(seed=6, secs=10.0), {}),
           ('synthetic part 7, Fast', C.synth_notes(seed=7, secs=10.0), {'response': 'Fast '}),
           ('bass line, Target -24', C.bassline(), {'target': -24.0}),
           ('bass line, Sensitivity 80', C.bassline(), {'sensitivity': 80.0}),
           ('bass line, Artifacts 20', C.bassline(), {'artifacts': 20.0}),
           ('synthetic part 8, range -6 .. +6', C.synth_notes(seed=8, secs=10.0), {'range_min': -6.0, 'range_max': 6.0})]
    return out[:4] if quick else out


def main():
    quick = '--quick' in sys.argv
    rows = []
    for name, x, kw in cases(quick):
        x = np.asarray(x, np.float32)
        xs = np.stack([x, x])
        y_ref = render_ref(xs, **kw)
        ours_kw = {k: v for k, v in kw.items()}
        y_ours = render_ours(xs, **ours_kw)
        s = int(0.1 * FS)
        res = 20 * np.log10(np.sqrt(np.mean((y_ours[:, s:] - y_ref[:, s:]) ** 2)) / np.sqrt(np.mean(y_ref[:, s:] ** 2)) + 1e-30)
        g_ref, g_ours = M.gtrace(x, y_ref[0]), M.gtrace(x, y_ours[0])
        env = np.convolve(x.astype(float) ** 2, np.ones(96) / 96, 'same'); ok = env > 10 ** (-55 / 10); ok[:s] = False
        d = np.abs(g_ours - g_ref)[ok]
        rows.append((name, res, np.median(d), np.percentile(d, 90)))
        print('%-36s residual %6.1f dB   gain error median %.3f dB, 90%% %.3f dB' % rows[-1], flush=True)
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
        with open(out, 'w', encoding='utf-8') as f:
            f.write('| case | residual (dB) | gain error median (dB) | 90 % (dB) |\n|---|---|---|---|\n')
            for r in rows: f.write('| %s | %.1f | %.3f | %.3f |\n' % r)


if __name__ == '__main__':
    main()
