"""WDelay Phase 4: H-Delay's Analog modes (its stage colours the input: Mix 0 gives the stage's output alone). The same
probe set as H-Comp's (ident_hc_analog.py), fitted with the same model (fit_hc_analog2.py, ANALOG_PREFIX=hd).

    python -I ident_hd_analog.py collect <mode>     build/hd_analog<mode>.npz
    python -I ident_hd_analog.py noise              build/hd_analog_noise.npz
    python -I ident_hd_analog.py noisetable         measurements/hdelay/analog_noise.csv
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import hdelay as H
import ident_hc_analog as A

FS = 48000
BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build')


def hd(x, mode):
    x = np.asarray(x, np.float32)
    return H.render(np.stack([x, x]) if x.ndim == 1 else x, FS, preroll=1.0, mix=0, feedback=0, analog="Mod%d" % mode)


def collect(mode):
    out = []
    for name, x in A.dataset_signals():
        x = x.astype(np.float32)
        out.append(dict(name=name, x=x, y=hd(x, mode)[0]))
    np.savez(os.path.join(BUILD, 'hd_analog%d.npz' % mode), cases=np.array(out, dtype=object))
    print('saved', len(out), 'cases', flush=True)


def noise():
    n = 4 * FS; t = np.arange(n) / FS
    res = {}
    for mode in (1, 2, 3, 4):
        y = hd(np.zeros(n, np.float32), mode).astype(np.float64)[:, FS // 2:]
        f, P = A._psd(y[0])
        x = (0.1 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
        yt = hd(x, mode)[0].astype(np.float64)[FS // 2:]
        tt = t[FS // 2:]
        B = np.stack([fn(2 * np.pi * 1000 * k * tt) for k in range(1, 20) for fn in (np.sin, np.cos)] + [np.ones_like(tt)], 1)
        e = yt - B @ np.linalg.lstsq(B, yt, rcond=None)[0]
        _, P2 = A._psd(e)
        res[mode] = dict(f=f, P=P, Pt=P2, corr=np.corrcoef(y[0], y[1])[0, 1], mean=y.mean(1))
        print('mode %d: noise rms %.1f dBFS, L/R corr %.3f; under a -20 dBFS tone %.1f dBFS' % (
            mode, 10 * np.log10(np.mean(y[0] ** 2) + 1e-30), res[mode]['corr'], 10 * np.log10(np.mean(e ** 2) + 1e-30)))
    np.savez(os.path.join(BUILD, 'hd_analog_noise.npz'), **{'%s%d' % (k, m): v for m, d in res.items() for k, v in d.items()})


def noise_table():
    d = np.load(os.path.join(BUILD, 'hd_analog_noise.npz'))
    edges = np.geomspace(20.0, 24000.0, 41)
    centres = np.sqrt(edges[:-1] * edges[1:])
    rows = []
    for k, (lo, hi) in enumerate(zip(edges[:-1], edges[1:])):
        row = [centres[k]]
        for m in (1, 2, 3, 4):
            f, P = d['f%d' % m], d['P%d' % m]
            sel = (f >= lo) & (f < hi)
            row.append(10 * np.log10(P[sel].mean()) if sel.any() else np.nan)
        rows.append(row)
    a = np.array(rows)
    for j in range(1, 5):
        bad = np.isnan(a[:, j]); a[bad, j] = np.interp(np.log(a[bad, 0]), np.log(a[~bad, 0]), a[~bad, j])
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hdelay', 'analog_noise.csv')
    np.savetxt(out, a, delimiter=',', header='hz,mode1_db,mode2_db,mode3_db,mode4_db', comments='', fmt='%.4f')
    print('wrote', out)


if __name__ == '__main__':
    {'collect': lambda: collect(int(sys.argv[2])), 'noise': noise, 'noisetable': noise_table}[sys.argv[1]]()
