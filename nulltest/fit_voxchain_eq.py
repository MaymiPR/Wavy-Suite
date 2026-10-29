"""Phase 4: Bass and Treble. For each colour, the reference's responses at all 21 fader steps over the chain with every
colour clear (complex, 1/12 octave, from campaign_voxchain.py's single.json) are fitted together with RBJ-style sections
(low / high shelves, bells, second-order high-passes) prewarped at 48 kHz: each section's frequency fixed, its Q a smooth
function of its gain, the gain per step, so the plug-in can interpolate the gains between steps (fitting each step on its
own found different equivalent shapes at neighbouring steps, which do not interpolate). Writes
plugins/WVoxChain/WVoxChainEq.h and prints each fit's worst error (dB) over 30 Hz .. 16 kHz.

    python -I fit_voxchain_eq.py
"""
import json
import os
import sys
import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from campaign_voxchain import FREQS
OUT = os.path.join(HERE, '..', 'plugins', 'WVoxChain', 'WVoxChainEq.h')
FS = 48000.0
BAND = (FREQS >= 25) & (FREQS <= 18000)
# each colour's sections: (type, initial frequency, initial Q); 'hp' a second-order high-pass (no gain)
LAYOUT = {
    ('bass', 1): [('ls', 60.0, 0.7), ('pk', 120.0, 0.8), ('hp', 40.0, 0.6)],
    ('bass', 2): [('ls', 100.0, 0.7), ('pk', 150.0, 0.8), ('hp', 40.0, 0.6)],
    ('bass', 3): [('ls', 100.0, 0.7), ('pk', 900.0, 0.7), ('hp', 40.0, 0.7), ('hp', 40.0, 1.3)],
    ('treble', 1): [('pk', 5000.0, 1.0), ('pk', 9000.0, 0.7)],
    ('treble', 2): [('hs', 5000.0, 0.7), ('pk', 12000.0, 0.7)],
    ('treble', 3): [('hs', 8000.0, 0.7), ('pk', 600.0, 0.7)],
}


def biquad(kind, f0, q, gdb, fs=FS):
    """RBJ cookbook coefficients (b, a) with the frequency prewarped (the bilinear transform's own)"""
    A = 10 ** (gdb / 40); w = 2 * np.pi * f0 / fs; cw, sw = np.cos(w), np.sin(w); al = sw / (2 * q)
    if kind == 'pk':
        b = [1 + al * A, -2 * cw, 1 - al * A]; a = [1 + al / A, -2 * cw, 1 - al / A]
    elif kind == 'ls':
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s)]
        a = [(A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s]
    elif kind == 'hs':
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s)]
        a = [(A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s]
    elif kind == 'hp':
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]; a = [1 + al, -2 * cw, 1 - al]
    b = np.array(b) / a[0]; a = np.array(a) / a[0]
    return b, a


def resp(b, a, f, fs=FS):
    z = np.exp(-1j * 2 * np.pi * f / fs)
    return (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)


def model(params, layout, f, hp_on=True):
    H = np.ones(len(f), complex)
    i = 0
    for kind, _, _ in layout:
        if kind == 'hp':
            fr, q = params[i], params[i + 1]; i += 2
            if hp_on: H *= resp(*biquad('hp', fr, q, 0.0), f)
        else:
            fr, q, g = params[i], params[i + 1], params[i + 2]; i += 3
            H *= resp(*biquad(kind, fr, q, g), f)
    return H


def fit_one(target, layout, init, hp_on):
    lo, hi = [], []
    for kind, f0, q in layout:
        lo += [10.0, 0.1]; hi += [22000.0, 8.0]
        if kind != 'hp': lo.append(-30.0); hi.append(30.0)
    def res(p):
        H = model(p, layout, FREQS, hp_on)
        d = np.log(H[BAND]) - np.log(target[BAND])
        d = np.concatenate([d.real * 8.686, np.angle(np.exp(1j * d.imag)) * 20.0])   # dB, and phase (rad x 20)
        return d
    init = np.clip(init, np.array(lo) + 1e-6, np.array(hi) - 1e-6)
    sol = least_squares(res, init, bounds=(lo, hi), x_scale='jac', max_nfev=4000)
    H = model(sol.x, layout, FREQS, hp_on)
    worst = np.max(np.abs(20 * np.log10(np.abs(H[BAND]) / np.abs(target[BAND]))))
    return sol.x, worst


def joint(layout, targets, hp_flags, start):
    """every step of a colour at once: per section a shared frequency and Q = q0 + q1 g + q2 g^2 (g: the section's gain
    at that step), the gain free per step (none for a high-pass: its Q constant)"""
    nst = len(targets)
    def unpack(th):
        out = []; i = 0
        for kind, _, _ in layout:
            if kind == 'hp': out.append((kind, th[i], th[i + 1], 0.0, 0.0, None)); i += 2
            else: out.append((kind, th[i], th[i + 1], th[i + 2], th[i + 3], th[i + 4:i + 4 + nst])); i += 4 + nst
        return out
    def step_params(secs, k):
        pr = []
        for kind, f, q0, q1, q2, g in secs:
            if kind == 'hp': pr += [f, q0]
            else: gg = g[k]; pr += [f, max(0.05, q0 + q1 * gg + q2 * gg * gg), gg]
        return np.array(pr)
    def res(th):
        secs = unpack(th); r = []
        for k in range(nst):
            H = model(step_params(secs, k), layout, FREQS, hp_flags[k])
            d = np.log(H[BAND]) - np.log(targets[k][BAND])
            r.append(np.concatenate([d.real * 8.686, np.angle(np.exp(1j * d.imag)) * 20.0]))
        return np.concatenate(r)
    sol = least_squares(res, start, x_scale='jac', max_nfev=3000)
    secs = unpack(sol.x)
    worst = []
    for k in range(nst):
        H = model(step_params(secs, k), layout, FREQS, hp_flags[k])
        worst.append(np.max(np.abs(20 * np.log10(np.abs(H[BAND]) / np.abs(targets[k][BAND])))))
    return sol.x, secs, worst


def main():
    S = json.load(open(os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'campaign', 'single.json')))
    clear = next(r for r in S if r['kind'] == 'tf' and r['stage'] == 'clear')
    H0 = 10 ** (np.array(clear['magL']) / 20) * np.exp(1j * np.array(clear['phL']))
    lines = ['#pragma once', '// Generated by nulltest/fit_voxchain_eq.py: Bass and Treble per colour, fitted jointly over the 21 fader steps',
             '// -10 .. +10 to the reference responses over the chain with every colour clear: per section a fixed frequency',
             '// (Hz), Q = q0 + q1 g + q2 g^2 of its gain g (dB) at that step, and the gain per step (a high-pass: Q = q0).',
             '// Do not edit.', 'namespace wvoxchain::eq', '{',
             'enum Kind { lowShelf, peak, highShelf, highPass };', 'struct Section { Kind kind; float f, q0, q1, q2; float g[21]; };',
             'struct Colour { int n; Section s[4]; bool hpFromZero; };   // hpFromZero: the high-pass only at fader 0 and above']
    kinds = {'ls': 'lowShelf', 'pk': 'peak', 'hs': 'highShelf', 'hp': 'highPass'}
    for (st, col), layout in LAYOUT.items():
        rows = sorted((r for r in S if r['kind'] == 'tf' and r['stage'] == st and r['colour'] == col), key=lambda r: r['fader'])
        targets = [10 ** (np.array(r['magL']) / 20) * np.exp(1j * np.array(r['phL'])) / H0 for r in rows]
        hp_flags = [not (st == 'bass' and col == 3 and r['fader'] < 0) for r in rows]
        # start: the per-step fit at +10 and -10 for the shapes, gains along 1.2 dB per step
        best = None
        rng = np.random.default_rng(col)
        for trial in range(6):
            th = []
            for kind, f0, q in layout:
                fr = f0 * (rng.uniform(0.6, 1.6) if trial else 1.0)
                if kind == 'hp': th += [fr, q]
                else: th += [fr, q, 0.0, 0.0] + [1.2 * (k - 10) * (rng.uniform(0.3, 1.0) if trial else (1.0 if kind != 'pk' else 0.3)) for k in range(21)]
            x, secs, worst = joint(layout, targets, hp_flags, np.array(th))
            if best is None or max(worst) < max(best[2]): best = (x, secs, worst)
        x, secs, worst = best
        print('%-6s colour %d: worst error per step %s (max %.2f dB)' % (st, col, ' '.join('%.2f' % w for w in worst), max(worst)), flush=True)
        out = []
        for kind, f, q0, q1, q2, g in secs:
            gg = g if g is not None else np.zeros(21)
            out.append('{ %s, %.4ff, %.5ff, %.6ff, %.7ff, { %s } }' % (kinds[kind], f, q0, q1, q2, ', '.join('%.4ff' % v for v in gg)))
        lines.append('inline constexpr Colour %s%d { %d, { %s }, %s };' % (st, col, len(layout), ', '.join(out), 'true' if (st, col) == ('bass', 3) else 'false'))
    lines += ['inline constexpr const Colour* bass[4] = { nullptr, &bass1, &bass2, &bass3 };',
              'inline constexpr const Colour* treble[4] = { nullptr, &treble1, &treble2, &treble3 };', '}', '']
    open(OUT, 'w').write('\n'.join(lines))
    print('wrote', OUT)


if __name__ == '__main__':
    main()
