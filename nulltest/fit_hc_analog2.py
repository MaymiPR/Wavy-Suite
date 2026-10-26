"""WComp Phase 4: the Analog stage, general form: y = L(x) + H2( g( H1(x) ) ), on the datasets of ident_hc_analog.py.

    python -I fit_hc_analog2.py <mode> [nfev]       writes build/hc_analog<mode>_fit2.json
    ANALOG_PREFIX=hd: H-Delay's datasets (ident_hd_analog.py), build/hd_analog<mode>_fit2.json, starting from H-Comp's
    fit of the same mode; "export" then writes measurements/hdelay/analog_model.json

L: gain times first-order sections; H1, H2: first-order sections (1 + s/wz) / (1 + s/wp) (bilinear); g a table over u
(piecewise linear, knots on a log-like scale, both signs: odd and even parts free).
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import optimize
import fit_hc_analog as F

BUILD = F.BUILD
PREFIX = os.environ.get('ANALOG_PREFIX', 'hc')
NL, N1, N2 = 2, 3, 2
POS = np.array([1e-4, 2e-4, 4e-4, 7e-4, 1e-3, 1.5e-3, 2.2e-3, 3.3e-3, 5e-3, 1e-2, 2e-2, 5e-2, 0.1, 0.2, 0.4, 0.7, 1.2])
KN = np.concatenate([-POS[::-1], [0.0], POS])


BP_MODES = tuple(int(m) for m in os.environ.get('ANALOG_BP', '3').split(','))   # H1 starts with a band-pass biquad (f0, Q): the
# broad dip of mode 3 (mode 2 fits as well without); H-Delay's mode 1 is such a dip too, low (ANALOG_BP=1,3)
MODE = 0


def bandpass(x, lf0, lq):
    from scipy import signal
    w0 = 2 * np.pi * np.exp(lf0); Q = np.exp(lq)
    b, a = signal.bilinear([1 / (Q * w0), 0.0], [1 / w0 ** 2, 1 / (Q * w0), 1.0], FS_)
    return signal.lfilter(b, a, x)


FS_ = 48000


def split(q):
    i = 0
    lL = q[i:i + 2 * NL]; i += 2 * NL
    l1 = q[i:i + 2 * N1]; i += 2 * N1
    l2 = q[i:i + 2 * N2]; i += 2 * N2
    gain = q[i]; i += 1
    tab = q[i:i + len(KN)]; i += len(KN)
    bp = q[i:i + 2]
    return lL, l1, l2, gain, tab, bp


def gfun(u, tab):
    # linear beyond the table ends (the last slope)
    y = np.interp(u, KN, tab)
    sl_hi = (tab[-1] - tab[-2]) / (KN[-1] - KN[-2]); sl_lo = (tab[1] - tab[0]) / (KN[1] - KN[0])
    y = np.where(u > KN[-1], tab[-1] + sl_hi * (u - KN[-1]), y)
    return np.where(u < KN[0], tab[0] + sl_lo * (u - KN[0]), y)


def model(q, x):
    lL, l1, l2, gain, tab, bp = split(q)
    u = F.cascade(bandpass(x, *bp) if len(bp) == 2 else x, l1)
    return (1 + gain) * F.cascade(x, lL) + F.cascade(gfun(u, tab - np.interp(0.0, KN, tab)), l2)


def q_init(mode):
    p = os.path.join(BUILD, 'hc_analog4_fit.json')
    q4 = np.array(json.load(open(p))['q'])
    l1 = q4[:6]; l2 = q4[6:10]; w, s, pp = np.exp(q4[10:13])
    u = KN / s
    tab = -w * u / (1 + np.abs(u) ** pp) ** (1 / pp) if mode == 4 else np.zeros(len(KN))
    lL = np.log([1e5, 1e5, 1e5, 1e5])
    if mode in BP_MODES:
        # the dip: about 0.43 of the signal subtracted around 2 kHz at low levels, saturating near 0.05 (mode 3);
        # a tenth of that for mode 2
        k, sat = (0.43, 0.05) if mode == 3 else (0.09, 0.01)
        tab = -k * KN / np.sqrt(1 + (k * KN / sat) ** 2)
        l1 = np.log([1e5, 1e5, 1e5, 1e5, 1e5, 1e5]); l2 = np.log([1e5, 1e5, 1e5, 1e5])
        return np.concatenate([lL, l1, l2, [0.0], tab, [np.log(2000.0), np.log(0.5)]])
    return np.concatenate([lL, l1, l2, [q4[13] if mode == 4 else 0.0], tab])


def residuals(q, cs, seg=slice(12000, 36000)):
    r = []
    for c in cs:
        x = c['x'].astype(np.float64); y = c['y'].astype(np.float64)
        m = model(q, x)
        r.append((y[seg] - m[seg]) / (np.std((y - x)[seg]) + 1e-4))
    return np.concatenate(r)


def report(q, cs):
    rows = []
    for c in cs:
        x = c['x'].astype(np.float64); y = c['y'].astype(np.float64); s = slice(12000, None)
        m = model(q, x)
        rows.append((20 * np.log10(np.std(y[s] - m[s]) / np.std(y[s])), 20 * np.log10(np.std(y[s] - x[s]) / np.std(y[s])), c['name']))
    rows.sort()
    for r in rows[::4] + rows[-6:]: print('  null %6.1f dB (vs no model %6.1f): %s' % r)
    print('median null %.1f dB' % np.median([r[0] for r in rows]))


def main():
    mode = int(sys.argv[1]); nfev = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    cs = F.load(mode)
    out = os.path.join(BUILD, '%s_analog%d_fit2.json' % (PREFIX, mode))
    start = os.path.join(BUILD, 'hc_analog%d_fit2.json' % mode)
    q = np.array(json.load(open(out if os.path.exists(out) else start))['q']) if os.path.exists(out) or os.path.exists(start) else q_init(mode)
    r = residuals(q, cs); print('start %.4f' % np.sqrt(np.mean(r ** 2)), flush=True)
    s = optimize.least_squares(residuals, q, args=(cs,), diff_step=1e-4, max_nfev=nfev, loss='soft_l1', f_scale=0.3)
    q = s.x; r = residuals(q, cs); print('fit %.4f' % np.sqrt(np.mean(r ** 2)))
    json.dump({'q': list(map(float, q)), 'NL': NL, 'N1': N1, 'N2': N2, 'knots': KN.tolist()}, open(out, 'w'))
    lL, l1, l2, gain, tab, bp = split(q)
    if len(bp): print('band-pass f0 %.1f Q %.3f' % tuple(np.exp(bp)))
    print('L f', np.round(np.exp(lL), 1), 'H1 f', np.round(np.exp(l1), 1), 'H2 f', np.round(np.exp(l2), 1), 'gain %.5f' % gain)
    print('g', np.round(tab * 1e3, 4))
    report(q, cs)


def export():
    """measurements/hcomp/analog_model.json from the fits (a mode not fitted yet: its starting point)."""
    modes = {}
    for m in (1, 2, 3, 4):
        p = os.path.join(BUILD, '%s_analog%d_fit2.json' % (PREFIX, m))
        if PREFIX == 'hd' and m == 4 and not os.path.exists(p): p = os.path.join(BUILD, 'hc_analog4_fit2.json')   # (H-Comp's)
        modes[str(m)] = json.load(open(p))['q'] if os.path.exists(p) else list(map(float, q_init(m)))
        print('mode', m, 'fitted' if os.path.exists(p) else 'NOT fitted (start values)')
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hdelay' if PREFIX == 'hd' else 'hcomp', 'analog_model.json')
    json.dump({'NL': NL, 'N1': N1, 'N2': N2, 'knots': KN.tolist(), 'modes': modes, 'bandpass_modes': list(BP_MODES),
               'model': 'y = (1 + gain) L(x) + H2(g(H1(x))) + noise; q = [L (log fz, log fp) x NL, H1 x N1, H2 x N2, gain, g on knots, (log f0, log Q of a band-pass before H1, bandpass_modes only)]'},
              open(out, 'w'), indent=1)
    print('wrote', out)


if __name__ == '__main__':
    export() if sys.argv[1] == 'export' else main()
