"""Training set for WComp's learned gain correction (nulltest/train_wcomp.py): random settings and signals rendered
through the reference H-Comp and through WComp's offline renderer, with WComp's states traced (WCompRender trace=).
Mix 100, Output cancelling the makeup (so neither limiter nor clip acts), Analog Off. The null test's settings and the
factory presets' settings are added on top of the random cases, so the training sees the cases that already null.

    python -I render_trainset_wcomp.py [random cases] [seed] [out dir name] [--random-only] [--hot]
    --hot: drums, bass, plucks and voice at -12 ... +3 dB peak (not normalised; Output 4 dB lower still)
    --common: the default, the null test's and the presets' settings on every signal, each at a random level -12 ... +3 dB
                                                     -> build/wcompnet/<data>/case_NNNN.npz

Each file: x (input), yW, yH (outputs), feat (steps of 16 samples x 11 inputs, see FEATURES), guard (per step),
settings (threshold, ratio, attack, release, punch, output).
"""
import sys, os, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wcomp as N, refhost as R

FS, STEP, LA = 48000, 16, 64
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'build', 'wcompnet', sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else 'data')
os.makedirs(OUT, exist_ok=True)
SLOPE = [0.0, 0.0, 0.010419785, 0.101765597, 0.163803536, 0.208603068, 0.268752098, 0.307228554, 0.333910439, 0.353488518,
         0.380254, 0.397682211, 0.419019904, 0.431577263, 0.445716596, 0.456189956, 0.464166211, 0.470516228]
RAT = [1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 8.0, 10.0, 14.0, 20.0, 30.0, 50.0]
FEATURES = ['held peak - threshold', 'target', 'R', 'S', 'reduction after the attack', 'reduction out',
            'ln attack', 'ln release', '1 / ratio', 'punch', 'threshold']


def makeup_db(thr, ratio):                     # WCompDsp.h makeupDb at Release 100 (the scale with release is < 4 %)
    inv = 1.0 / np.array(RAT); k = np.interp(1.0 / ratio, inv[::-1], np.array(SLOPE)[::-1])
    return k * max(0.0, -9.0 - thr)


def features(trace, st):
    thr, rat, att, rel, pu = st[:5]
    tr = trace.reshape(-1, 6)[LA:]             # aligned with the output (the latency first)
    tr = tr[::STEP]
    h, t, Rr, S, g1, g = [tr[:, i].astype(np.float64) for i in range(6)]
    f = np.stack([np.clip(20 * np.log10(h + 1e-5) - thr, -40, 40) / 24, t / 24, Rr / 24, S / 24,
                  -20 * np.log10(np.maximum(g1, 1e-6)) / 24, -20 * np.log10(np.maximum(g, 1e-6)) / 24,
                  np.full_like(h, np.log(att) / np.log(100)), np.full_like(h, np.log(rel) / np.log(3000)),
                  np.full_like(h, 1.0 / rat), np.full_like(h, pu / 30), np.full_like(h, thr / 48)], 1)
    guard = np.clip((Rr + (-20 * np.log10(np.maximum(g, 1e-6)))) / 1.0, 0, 1)   # no correction without reduction
    return f.astype(np.float32), guard.astype(np.float32)


def signal_pool(rng):
    S = N.signals(FS)
    pool = dict(S)
    t = np.arange(4 * FS) / FS
    for k in range(6):                         # low tones and plucked low notes (the bass-heavy weak cases)
        f0 = float(rng.uniform(35, 300))
        pool['low tone %d' % k] = (0.5 * np.sin(2 * np.pi * f0 * t)).astype(np.float32)
        env = np.exp(-((t * 4) % 1) / float(rng.uniform(0.05, 0.4)))
        pool['pluck %d' % k] = (0.6 * env * np.sign(np.sin(2 * np.pi * f0 * t)) * np.abs(np.sin(2 * np.pi * f0 * t)) ** 0.5).astype(np.float32)
    return pool


def render_case(i, st, name, x, extra_out=0.0):
    thr, rat, att, rel, pu = st
    out = max(-18.0, -round(makeup_db(thr, rat), 1) - extra_out)   # the Output range ends at -18 dB
    xs = np.stack([x, x]).astype(np.float32)
    fi, fo, ft = [os.path.join(N.TMP, '%s%d.f32' % (p, os.getpid())) for p in ('tin', 'tout', 'ttr')]
    xs.T.tofile(fi)
    subprocess.run([N.EXE, fi, fo, '2', str(FS), 'threshold=%g' % thr, 'ratio=%g' % rat, 'attack=%g' % att, 'release=%g' % rel,
                    'punch=%g' % pu, 'mix=100', 'output=%g' % out, 'limiter=0', 'analog=0', 'learned=0', 'trace=' + ft], check=True, capture_output=True)
    yW = np.fromfile(fo, np.float32).reshape(-1, 2).T[0]
    yH = N.theirs(xs, FS, thr, rat, att, rel, pu, 100.0, out, 'Off')[0]
    feat, guard = features(np.fromfile(ft, np.float32), st)
    np.savez_compressed(os.path.join(OUT, 'case_%04d.npz' % i), x=x, yW=yW, yH=yH, feat=feat, guard=guard,
                        settings=np.array([thr, rat, att, rel, pu, out]), name=name)


def main():
    import nulltest_wcomp_presets as P
    n_rand = int(sys.argv[1]) if len(sys.argv) > 1 else 700
    rng = np.random.default_rng(int(sys.argv[2]) if len(sys.argv) > 2 else 11)
    pool = signal_pool(rng); names = list(pool)
    cases = []
    fixed = '--random-only' not in sys.argv
    for st in (N.SETTINGS[:8] if fixed else []):   # the null test (Analog Off), every signal
        for name in names[:14]: cases.append((st[:5], name, 1.0))
    for group, pname, v in (P.presets() if fixed else []):   # the presets' settings, four signals (synced: as ms)
        sync = int(v['sync'])
        rel = v['release'] if sync == 2 else P.synced_ms(int(v['releaseNote']), v['bpm'] if sync == 0 else 120.0)
        st = (v['threshold'], v['ratio'], v['attack'], round(rel, 2), v['punch'])
        for name in ('voice-like', 'drum loop', 'bass line', 'pink noise -20 rms'): cases.append((st, name, 1.0))
    hot = '--hot' in sys.argv
    hot_names = [k for k in names if k.startswith(('drum', 'bass', 'pluck', 'voice', 'low tone'))]
    for _ in range(n_rand):
        st = (round(float(rng.uniform(-48, -6)), 1), round(float(np.exp(rng.uniform(np.log(1.2), np.log(50)))), 2),
              round(float(np.exp(rng.uniform(np.log(0.5), np.log(100)))), 2), round(float(np.exp(rng.uniform(np.log(3), np.log(3000)))), 1),
              round(float(rng.uniform(0, 30) if rng.random() < 0.4 else rng.uniform(0, 3)), 1))
        if hot:
            name = hot_names[int(rng.integers(len(hot_names)))]
            cases.append((st, name, float(10 ** (rng.uniform(-12, 3) / 20) / max(1e-9, np.abs(pool[name]).max()))))
        else:
            cases.append((st, names[int(rng.integers(len(names)))], float(10 ** (rng.uniform(-18, 2) / 20))))
    if '--common' in sys.argv:                 # the settings people use most, densely: every signal, random levels
        hot = True
        common = [st[:5] for st in N.SETTINGS[:8]]
        for group, pname, v in P.presets():
            sync = int(v['sync'])
            rel = v['release'] if sync == 2 else P.synced_ms(int(v['releaseNote']), v['bpm'] if sync == 0 else 120.0)
            common.append((v['threshold'], v['ratio'], v['attack'], round(rel, 2), v['punch']))
        cases = [(st, name, float(10 ** (rng.uniform(-12, 3) / 20) / max(1e-9, np.abs(pool[name]).max()))) for st in common for name in names]
    print(len(cases), 'cases', flush=True)
    for i, (st, name, gain) in enumerate(cases):
        path = os.path.join(OUT, 'case_%04d.npz' % i)
        if os.path.exists(path): continue
        x = pool[name][:3 * FS] * gain
        x = (x * (1.0 if hot else min(1.0, 0.9 / max(1e-9, np.abs(x).max())))).astype(np.float32)
        render_case(i, st, name, x, 4.0 if hot else 0.0)
        if i % 25 == 0: print(i, st, name, flush=True)


if __name__ == '__main__':
    main()
