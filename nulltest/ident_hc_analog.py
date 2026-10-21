"""WComp Phase 4: identify H-Comp's Analog modes (compressor idle: threshold 0, ratio 1).

    python -I ident_hc_analog.py probe <mode>     determinism, memory, static curve
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

FS = 48000
IDLE = dict(threshold=0.0, ratio=1.0, output=0.0, mix=100.0)


def hc(x, mode, **kw):
    return R.render_hc(x, FS, analog=mode, **{**IDLE, **kw})


def probe(mode):
    rng = np.random.default_rng(1)
    n = FS * 2
    w = (0.05 * rng.standard_normal(n)).astype(np.float32)
    y1 = hc(w, mode)[0]; y2 = hc(w, mode)[0]
    print('determinism: rms diff %.3g (signal rms %.3g)' % (np.sqrt(np.mean((y1 - y2) ** 2)), np.sqrt(np.mean(y1 ** 2))))
    # linear part: cross-spectrum against the input at a low and a high level
    for a in (0.003, 0.05, 0.5):
        x = (a * rng.standard_normal(n)).astype(np.float32)
        y = hc(x, mode)[0].astype(np.float64)
        # least-squares FIR of 64 taps centred (lag -16..47)
        L = 64; off = 16
        X = np.stack([np.roll(x, k - off) for k in range(L)], 1)[2000:-2000]
        h, *_ = np.linalg.lstsq(X, y[2000:-2000], rcond=None)
        e = y[2000:-2000] - X @ h
        print('level %.3f: fir peak tap %d (%.4f), sum %.4f, residual %.1f dB re output' % (
            a, np.argmax(np.abs(h)) - off, h[np.argmax(np.abs(h))], h.sum(), 20 * np.log10(np.std(e) / np.std(y))))
    # static curve: a slow sine, y against x
    t = np.arange(FS) / FS
    x = (0.02 * np.sin(2 * np.pi * 50 * t)).astype(np.float32)
    y = hc(x, mode)[0]
    np.savez(os.path.join(R.__file__.rsplit(os.sep, 1)[0], '..', 'build', 'hc_analog%s_probe.npz' % mode), x=x, y=y)
    d = y - x
    for v in (0.0005, 0.001, 0.002, 0.005, 0.01, 0.02):
        k = np.argmin(np.abs(x[:FS // 100] - v))
        print('  x %.4f  y %.6f  y-x %.2e' % (x[k], y[k], d[k]))


if __name__ == '__main__':
    if sys.argv[1] == "probe": probe(int(sys.argv[2]))


BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build')


def dataset_signals():
    import nulltest_wcomp as N
    rng = np.random.default_rng(11)
    n = int(1.5 * FS); t = np.arange(n) / FS
    S = []
    for a in (0.001, 0.003, 0.01, 0.05, 0.3):
        for f in (20.0, 60.0, 200.0, 700.0, 2000.0, 6000.0, 15000.0):
            S.append(('sine %g %g' % (a, f), a * np.sin(2 * np.pi * f * t)))
    for r in (0.001, 0.003, 0.01, 0.05, 0.2):
        S.append(('white %g' % r, r * rng.standard_normal(n)))
        p = np.fft.irfft(np.fft.rfft(rng.standard_normal(n)) / np.sqrt(np.maximum(1, np.arange(n // 2 + 1))), n)
        S.append(('pink %g' % r, r * p / np.std(p)))
    P = N.signals(FS, quick=False)
    for name in ('voice-like', 'drum loop', 'bass line'):
        v = P[name][:n].astype(np.float64)
        for pk in (0.03, 0.3, 0.7):
            S.append(('%s %g' % (name, pk), v * pk / np.abs(v).max()))
    return S


def collect(mode):
    out = []
    for name, x in dataset_signals():
        x = x.astype(np.float32)
        y = hc(x, mode)[0]
        out.append(dict(name=name, x=x, y=y))
    np.savez(os.path.join(BUILD, 'hc_analog%d.npz' % mode), cases=np.array(out, dtype=object))
    print('saved', len(out), 'cases')


if __name__ == '__main__' and sys.argv[1] == 'collect':
    collect(int(sys.argv[2]))


def noise():
    """The Analog modes' noise with silence in and under a tone (the tone removed by a fit of its harmonics)."""
    n = 4 * FS; t = np.arange(n) / FS
    res = {}
    for mode in (1, 2, 3, 4):
        z = np.zeros((2, n), np.float32)
        y = hc(z, mode).astype(np.float64)[:, FS // 2:]
        f, P = _psd(y[0])
        corr = np.corrcoef(y[0], y[1])[0, 1]
        mean = y.mean(1)
        # with a 1 kHz tone at -20 dBFS: remove its harmonics by least squares, keep the rest
        x = (0.1 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
        yt = hc(x, mode)[0].astype(np.float64)[FS // 2:]
        tt = t[FS // 2:]
        B = np.stack([fn(2 * np.pi * 1000 * k * tt) for k in range(1, 20) for fn in (np.sin, np.cos)] + [np.ones_like(tt)], 1)
        e = yt - B @ np.linalg.lstsq(B, yt, rcond=None)[0]
        f2, P2 = _psd(e)
        res[mode] = dict(f=f, P=P, Pt=P2, corr=corr, mean=mean)
        print('mode %d: noise rms %.1f dBFS, L/R corr %.3f, DC %.2e; under a -20 dBFS tone: %.1f dBFS' % (
            mode, 10 * np.log10(np.mean(y[0] ** 2) + 1e-30), corr, mean[0], 10 * np.log10(np.mean(e ** 2) + 1e-30)))
    np.savez(os.path.join(BUILD, 'hc_analog_noise.npz'), **{'%s%d' % (k, m): v for m, d in res.items() for k, v in d.items()})


def _psd(v):
    from scipy import signal
    return signal.welch(v, FS, nperseg=4096)


if __name__ == '__main__' and sys.argv[1] == 'noise':
    noise()


def noise_table():
    """measurements/hcomp/analog_noise.csv: the noise PSD (dB re 1/Hz, one-sided) per mode in log bands."""
    d = np.load(os.path.join(BUILD, 'hc_analog_noise.npz'))
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
    for j in range(1, 5):                                   # bands narrower than a bin: interpolate
        bad = np.isnan(a[:, j]); a[bad, j] = np.interp(np.log(a[bad, 0]), np.log(a[~bad, 0]), a[~bad, j])
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hcomp', 'analog_noise.csv')
    np.savetxt(out, a, delimiter=',', header='hz,mode1_db,mode2_db,mode3_db,mode4_db', comments='', fmt='%.4f')
    print('wrote', out)


if __name__ == '__main__' and sys.argv[1] == 'noisetable':
    noise_table()
