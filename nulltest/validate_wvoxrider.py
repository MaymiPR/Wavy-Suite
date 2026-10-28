"""Phase 5 checks for WVox Rider: (1) the built VST3 (hosted by tools/RefRender) against the offline renderer
(WVoxRiderRender, the same processor), stereo, at 48 and 96 kHz; (2) CPU. Prints a table for docs/VALIDATION.md.
(The reference against WVox Rider at other rates, in Stereo and in Live: nulltest_wvoxrider.py.)

    python -I validate_wvoxrider.py
"""
import sys, os, time, subprocess, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import resample_poly
import refhost as R, vocal_corpus as V, nulltest_wvoxrider as N

HERE = os.path.dirname(os.path.abspath(__file__))
VST3 = os.path.join(HERE, '..', 'build', 'plugins', 'WVoxRider', 'WVoxRider_artefacts', 'Release', 'VST3', 'WVox Rider.vst3')


def vst3(x, fs, params):
    """WVox Rider's VST3 through RefRender (stereo in / out), parameters by their display text"""
    d = tempfile.mkdtemp(prefix='vrv_'); fin, fout = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    x.T.astype(np.float32).tofile(fin)
    cmd = [R.REFRENDER, VST3, 'WVox Rider', fin, fout, 'fs=%d' % fs, 'block=512'] + ['%s=%s' % kv for kv in params.items()]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if not os.path.exists(fout): raise RuntimeError(r.stdout + r.stderr)
    return np.fromfile(fout, np.float32).reshape(-1, 2).T


def offline(x, fs, keys):
    d = tempfile.mkdtemp(prefix='vro_'); fi, fo = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    x.T.astype(np.float32).tofile(fi)
    subprocess.run([N.EXE, fi, fo, '2', str(fs), 'mode=0'] + ['%s=%g' % kv for kv in keys.items()], check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, 2).T


def res_db(y, r):
    a = 4800
    return 10 * np.log10(np.sum((y[:, a:] - r[:, a:]) ** 2) / np.sum(r[:, a:] ** 2) + 1e-30)


def main():
    v = V.part(24)[0].astype(np.float32)
    rows = []
    for fs in (48000, 96000):
        x = v if fs == 48000 else resample_poly(v, 2, 1).astype(np.float32)
        xs = np.vstack([x, 0.6 * np.roll(x, 2000)]).astype(np.float32)
        for label, p_txt, p_keys in (('defaults', {}, {}),
                                      ('Fast, Target -30', {'Attack': 'Fast', 'Target': '-30'}, {'attack': 1, 'target': -30}),
                                      ('Range -12 .. +12, Vocal Sensitivity 6', {'Range Max': '12', 'Range Min': '-12', 'Vocal Sensitivity': '6'},
                                       {'rangeMax': 12, 'rangeMin': -12, 'vocalSensitivity': 6})):
            yv = vst3(xs, fs, p_txt); yo = offline(xs, fs, p_keys)
            m = min(yv.shape[1], yo.shape[1])
            rows.append(('VST3 vs offline renderer, %s, %g kHz' % (label, fs / 1000), res_db(yv[:, :m], yo[:, :m])))
            print(rows[-1], flush=True)
    xs = np.vstack([v, v]); x = np.tile(xs, (1, 3)); t0 = time.perf_counter(); offline(x, 48000, {}); dt = time.perf_counter() - t0
    rows.append(('CPU: 60 s of stereo at 48 kHz rendered in (s; includes start-up)', dt)); print('cpu %.2f s = %.2f %%' % (dt, dt / 60 * 100))
    for k, val in rows: print('| %s | %.1f |' % (k, val))


if __name__ == '__main__':
    main()
