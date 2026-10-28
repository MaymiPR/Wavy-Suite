"""Phase 5 checks for WBass Rider: (1) the built VST3 (hosted by tools/RefRender) against the offline renderer
(WBassRiderRender, the same processor), (2) the reference against WBass Rider at 44.1 and 96 kHz, in the Live
components and in stereo, (3) CPU. Prints a table for docs/VALIDATION.md.

    python -I validate_wbassrider.py
"""
import sys, os, time, subprocess, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import resample_poly
import refhost as R, measure_bassrider as M, nulltest_wbassrider as N, brcompare as C, bass_corpus as K

HERE = os.path.dirname(os.path.abspath(__file__))
VST3 = os.path.join(HERE, '..', 'build', 'plugins', 'WBassRider', 'WBassRider_artefacts', 'Release', 'VST3', 'WBass Rider.vst3')


def res_db(x, y, r):
    """the residual over the loud parts, dB (x: the input, mono or (ch, n))"""
    xm = np.atleast_2d(x).mean(0)
    env = np.convolve(xm.astype(float) ** 2, np.ones(96) / 96, 'same'); ok = env > 1e-6; ok[:4800] = False
    y, r = np.atleast_2d(y), np.atleast_2d(r)
    return 10 * np.log10(np.sum((y - r)[:, ok] ** 2) / np.sum(r[:, ok] ** 2) + 1e-30)


def vst3(x, fs, params):
    """WBass Rider's VST3 through RefRender (stereo in / out), parameters by their display text"""
    d = tempfile.mkdtemp(prefix='brv_'); fin, fout = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    x = np.atleast_2d(np.asarray(x, np.float32)); x = np.vstack([x, x]) if x.shape[0] == 1 else x
    x.T.astype(np.float32).tofile(fin)
    cmd = [R.REFRENDER, VST3, 'WBass Rider', fin, fout, 'fs=%d' % fs, 'block=512'] + ['%s=%s' % kv for kv in params.items()]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if not os.path.exists(fout): raise RuntimeError(r.stdout + r.stderr)
    return np.fromfile(fout, np.float32).reshape(-1, 2).T


def ours(x, fs, **st):
    x = np.atleast_2d(np.asarray(x, np.float32))
    return N.render_ours(x, **st) if fs == 48000 else render_fs(x, fs, **st)


def render_fs(x, fs, **st):
    ch, n = x.shape
    d = tempfile.mkdtemp(prefix='brf_'); fi, fo = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    x.T.astype(np.float32).tofile(fi)
    args = [N.EXE, fi, fo, str(ch), str(fs), 'mode=%d' % (0 if ch == 2 else 1)]
    for k, v in st.items():
        if k == 'mode': args[-1] = 'mode=%d' % v
        elif k == 'response': args.append('response=%d' % (1 if v.strip() == 'Fast' else 0))
        else: args.append('%s=%g' % (N.KEYS[k], v))
    subprocess.run(args, check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, ch).T


def main():
    bass = C.bassline().astype(np.float32)
    rows = []
    # (1) the VST3 against the offline renderer, stereo, at 48 and 96 kHz
    for fs in (48000, 96000):
        x = bass if fs == 48000 else resample_poly(bass, 2, 1).astype(np.float32)
        xs = np.vstack([x, 0.5 * np.roll(x, 1000)])
        for label, p_txt, p_ours in (('defaults', {}, {}), ('Fast, Sensitivity 75', {'Response': 'Fast', 'Sensitivity': '75'}, {'response': 'Fast ', 'sensitivity': 75.0}),
                                      ('Artifacts 100, Range -6 .. +6', {'Artifacts': '100', 'Range Max': '6', 'Range Min': '-6'}, {'artifacts': 100.0, 'range_max': 6.0, 'range_min': -6.0})):
            yv = vst3(xs, fs, p_txt); yo = render_fs(xs, fs, **p_ours)
            m = min(yv.shape[1], yo.shape[1])
            rows.append(('VST3 vs offline renderer, %s, %g kHz' % (label, fs / 1000), res_db(xs[:, :m], yv[:, :m], yo[:, :m])))
            print(rows[-1], flush=True)
    # (2) the reference against WBass Rider: other rates (the bass line resampled), Live, stereo
    for fs in (44100, 96000):
        x = resample_poly(bass, 147, 160).astype(np.float32) if fs == 44100 else resample_poly(bass, 2, 1).astype(np.float32)
        R.fresh(M.NAME); p = R.load(M.NAME); r = p(x[None], fs, buffer_size=512, reset=True)[0]
        y = render_fs(x[None], fs)[0]
        rows.append(('reference vs WBass Rider, bass line at %g kHz' % (fs / 1000), res_db(x, y, r))); print(rows[-1], flush=True)
    for label, name, mode, st in (('Live Mono, bass line', 'Bass Rider Live Mono', 3, {}), ('Live Mono, bass line, Fast', 'Bass Rider Live Mono', 3, {'response': 'Fast '})):
        r = M.render(bass, name=name, **st); y = render_fs(bass[None], 48000, mode=mode, **st)[0]
        rows.append(('reference vs WBass Rider, %s' % label, res_db(bass, y, r))); print(rows[-1], flush=True)
    xs = np.vstack([bass, K.part(30)[0][:len(bass)] * 0.7]).astype(np.float32)
    for label, name, mode in (('Stereo, two different bass parts (L / R)', 'Bass Rider Stereo', 0), ('Live Stereo, the same', 'Bass Rider Live Stereo', 2)):
        r = M.render(xs, name=name); y = render_fs(xs, 48000, mode=mode)
        rows.append(('reference vs WBass Rider, %s' % label, res_db(xs, y, r))); print(rows[-1], flush=True)
    # (3) CPU: 60 s of stereo through the offline renderer, 512-sample blocks
    x = np.tile(xs, (1, 6)); t0 = time.perf_counter(); render_fs(x, 48000); dt = time.perf_counter() - t0
    rows.append(('CPU: 60 s of stereo at 48 kHz rendered in (s; % of one core in real time)', dt)); print('cpu %.2f s = %.2f %%' % (dt, dt / 60 * 100))
    for k, v in rows: print('| %s | %.1f |' % (k, v))


if __name__ == '__main__':
    main()
