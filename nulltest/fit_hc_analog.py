"""WComp Phase 4: fit the Analog stage, y = x + H2( g( H1(x) ) ) (a Wiener-Hammerstein offset added to the signal),
on the datasets of ident_hc_analog.py collect.

    python -I fit_hc_analog.py <mode> [nfev]       writes build/hc_analog<mode>_fit.json

H1, H2: cascades of first-order sections (1 + s/wz) / (1 + s/wp) (bilinear); g(u) = -w u / (1 + |u|^p)^(1/p).
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal, optimize

FS = 48000
BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build')
N1, N2 = 3, 2


def section(fz, fp):
    wz, wp = 2 * np.pi * fz, 2 * np.pi * fp
    # prewarped bilinear of (1 + s/wz) / (1 + s/wp)
    k = 2 * FS
    b = np.array([1 + k / wz, 1 - k / wz]); a = np.array([1 + k / wp, 1 - k / wp])
    return b / a[0], a / a[0]


def cascade(x, logf):
    for i in range(0, len(logf), 2):
        b, a = section(np.exp(logf[i]), np.exp(logf[i + 1]))
        x = signal.lfilter(b, a, x)
    return x


def model(q, x):
    l1, l2 = q[:2 * N1], q[2 * N1:2 * N1 + 2 * N2]
    lw, ls, lp, gl = q[2 * N1 + 2 * N2:2 * N1 + 2 * N2 + 4]
    u = cascade(x, l1) / np.exp(ls)
    p = np.exp(lp)
    g = -np.exp(lw) * u / (1 + np.abs(u) ** p) ** (1 / p)
    return x * (1 + gl) + cascade(g, l2)


def q0():
    # H1: a lead around 30-100 Hz and a lag above ~1.5 kHz; H2 flat
    l1 = np.log([30.0, 300.0, 1e5, 1500.0, 1e5, 1e5])
    l2 = np.log([1e5, 1e5, 1e5, 1e5])
    return np.concatenate([l1, l2, [np.log(0.001), np.log(0.0007), np.log(2.0), 0.0]])


def load(mode):
    cs = list(np.load(os.path.join(BUILD, '%s_analog%d.npz' % (os.environ.get('ANALOG_PREFIX', 'hc'), mode)), allow_pickle=True)['cases'])
    return [c for c in cs if np.abs(c['x']).max() < 0.75]


def residuals(q, cs, seg=slice(12000, 36000)):
    r = []
    for c in cs:
        x = c['x'].astype(np.float64); y = c['y'].astype(np.float64)
        m = model(q, x)
        d = y - x
        r.append((y[seg] - m[seg]) / (np.std(d[seg]) + 1e-4))
    return np.concatenate(r)


def report(q, cs):
    rows = []
    for c in cs:
        x = c['x'].astype(np.float64); y = c['y'].astype(np.float64); s = slice(12000, None)
        m = model(q, x)
        rows.append((20 * np.log10(np.std(y[s] - m[s]) / np.std(y[s])), 20 * np.log10(np.std(y[s] - x[s]) / np.std(y[s])), c['name']))
    for r in sorted(rows): print('  null %6.1f dB (vs no model %6.1f): %s' % r)
    print('median null %.1f dB' % np.median([r[0] for r in rows]))


if __name__ == '__main__':
    mode = int(sys.argv[1]); nfev = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    cs = load(mode)
    out = os.path.join(BUILD, 'hc_analog%d_fit.json' % mode)
    q = np.array(json.load(open(out))['q']) if os.path.exists(out) else q0()
    r = residuals(q, cs); print('start %.4f' % np.sqrt(np.mean(r ** 2)), flush=True)
    s = optimize.least_squares(residuals, q, args=(cs,), diff_step=1e-4, max_nfev=nfev, loss='soft_l1', f_scale=0.3)
    q = s.x; r = residuals(q, cs); print('fit %.4f' % np.sqrt(np.mean(r ** 2)))
    json.dump({'q': list(map(float, q)), 'N1': N1, 'N2': N2}, open(out, 'w'))
    print('H1 f', np.round(np.exp(q[:2 * N1]), 1), 'H2 f', np.round(np.exp(q[2 * N1:2 * N1 + 2 * N2]), 1),
          'w %.6f s %.6f p %.3f gain %.5f' % (np.exp(q[-4]), np.exp(q[-3]), np.exp(q[-2]), q[-1]))
    report(q, cs)
