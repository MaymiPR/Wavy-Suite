# Measures the reference Hybrid-line knobs (reference/wcomp/knobs, git-ignored captures) and writes the shading model
# that wavy::hybrid::Knob renders: Source/common/WavyHybridKnobData.h.
#
# Each knob body is described in polar coordinates around its centre. For every radial bin the colour is fitted, per
# channel, as a smooth function of the angle: a constant, three harmonics, two lobes around the highlight direction and
# the flute harmonic (the 10 lobes of the fluted rim). The fluted edge of the large knobs is fitted as a radius that
# varies with the angle; the cast shadow as a darkening of the panel. Only these smooth fits are kept: no pixels of
# the reference are stored. The pointer fin is drawn separately (see WavyHybridUI.cpp).
#
#   python tools/fit_hybrid_knobs.py      (numpy, Pillow; run from the repository root)
import math, os, re
import numpy as np
from scipy import ndimage, optimize
from PIL import Image

KN = 'reference/wcomp/knobs'
LIGHT = math.radians(320.0)          # highlight direction (JUCE angle: 0 up, clockwise), measured
NLOBE = 10

def load(i):
    return np.asarray(Image.open('%s/f_%d_after.png' % (KN, i)).convert('RGB')).astype(float)

def bil(I, x, y):
    # sample at continuous coordinates (pixel centres at i + 0.5)
    x = x - 0.5; y = y - 0.5
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = x - x0; fy = y - y0
    if I.ndim == 3: fx = fx[..., None]; fy = fy[..., None]
    return (I[y0, x0] * (1 - fx) * (1 - fy) + I[y0, x0 + 1] * fx * (1 - fy)
            + I[y0 + 1, x0] * (1 - fx) * fy + I[y0 + 1, x0 + 1] * fx * fy)

def wrap(a): return np.angle(np.exp(1j * a))

def basis(th, tk=None, extra=False):
    # th: the angle on screen (the light's frame); tk: the angle on the turned knob (the flutes' frame)
    if tk is None: tk = th
    d = wrap(th - LIGHT)
    cols = [np.ones_like(th)]
    for k in (1, 2, 3): cols += [np.cos(k * th), np.sin(k * th)]
    cols += [np.exp(-(d / math.radians(11)) ** 2), np.exp(-(d / math.radians(28)) ** 2)]
    g = np.exp(-(d / math.radians(40)) ** 2)
    cols += [np.cos(NLOBE * tk), np.sin(NLOBE * tk), g * np.cos(NLOBE * tk), g * np.sin(NLOBE * tk)]
    if extra:                        # sharper flutes (H-Delay's knobs): their second and third harmonics
        cols += [np.cos(2 * NLOBE * tk), np.sin(2 * NLOBE * tk), np.cos(3 * NLOBE * tk), np.sin(3 * NLOBE * tk)]
    return np.stack(cols, axis=-1)
NB = 13
STILL = 8.0                          # the flute terms reach this far inside the rim (the renderer redraws that ring)

def panel(x, y):
    # the panel model of drawPanel (no grain), in editor coordinates
    v = np.where(y < 28, 1.115, np.where(y < 170, 1.115 - 0.115 * (y - 28) / 142, np.where(y < 385, 1.0, 1.0 - 0.36 * ((y - 385) / 69) ** 2)))
    d = np.minimum(x, 642 - x)
    h = np.where(d >= 52, 1.0, 0.76 + 0.24 * np.sqrt(np.clip(d, 0, 52) / 52))
    return np.stack([187 * v * h, 105 * v * h, 39 * v * h], axis=-1)

# H-Delay's panel (fitted on reference/hdelay, as drawPanel's HDelay model): a base colour with a broad warm highlight at
# the upper left, darkened toward the bottom and the side edges, a bright line along the top
HD_PANEL = (183.5, 100.5, 35.97, 42.52, 55.87, 15.76, 107.1, 73.12, 100.4, 105.7, 229.3, 0.2535, 0.229, 22.65, 0.2993, 20.33, 0.7836)

def panel_hd(x, y):
    Br, Bg, Bb, Hr, Hg, Hb, x0, y0, sx, sy, yb, vb, el, wl, er, wr, tg = HD_PANEL
    blob = np.exp(-((x - x0) / sx) ** 2 - ((y - y0) / sy) ** 2)
    k = (1 - vb * np.clip((y - yb) / (292 - yb), 0, 1) ** 2) * (1 - el * np.exp(-x / wl)) * (1 - er * np.exp(-(749 - x) / wr))         * (1 + tg * np.exp(-(y - 19) / 3))
    return np.stack([(Br + Hr * blob) * k, (Bg + Hg * blob) * k, (Bb + Hb * blob) * k], axis=-1)

def lsq(X, Y, w=None, iters=3):
    # robust least squares (clips outliers such as ticks, scratches and print)
    keep = np.ones(len(Y), bool)
    for _ in range(iters):
        c = np.linalg.lstsq(X[keep], Y[keep], rcond=None)[0]
        res = np.abs(X @ c - Y).mean(axis=1) if Y.ndim > 1 else np.abs(X @ c - Y)
        s = np.median(res[keep]) * 3.0 + 2.0
        keep = res < s
    return c

def pointer_mask(T, R, J, r0):
    # the pointer fin and its shadow: an angular window around the pointer, outside r0
    return (np.abs(wrap(T - J)) < np.radians(14)) & (R > r0)

def turn(f):
    # a frame: (image, pointer angle) or (image, pointer angle, the flutes' own turn when it differs from the pointer's)
    return f[2] if len(f) > 2 else f[1]

def edge_fit(frames, cx, cy, r0, r1, rfin):
    # the body's edge radius against the angle, jointly over the frames, the flutes turned by each frame's turn
    th = np.radians(np.arange(0, 360, 0.5)); r = np.arange(r0, r1, 0.1)
    E, TK = [], []
    for f in frames:
        I, J, K = f[0], f[1], turn(f)
        L = I.mean(axis=2)
        T, R = np.meshgrid(th, r, indexing='ij'); P = bil(L, cx + R * np.sin(T), cy - R * np.cos(T))
        g = np.diff(P, axis=1); e = r[np.argmax(g, axis=1)] + 0.05
        keep = np.abs(wrap(th - J)) > np.radians(14)
        E.append(e[keep]); TK.append(th[keep] - K)
    e = np.concatenate(E); tk = np.concatenate(TK)
    X = np.stack([np.ones_like(tk)] + sum([[np.cos(k * NLOBE * tk), np.sin(k * NLOBE * tk)] for k in (1, 2, 3)], []), axis=-1)
    c = lsq(X, e)
    return np.concatenate([[c[0], 0.0, 0.0], c[1:]])      # e0, -, -, a10, b10, a20, b20, a30, b30 (in the knob's frame)

def edge_eval(c, tk):
    e = c[0] + 0 * tk
    for k in (1, 2, 3): e = e + c[2 * k + 1] * np.cos(k * NLOBE * tk) + c[2 * k + 2] * np.sin(k * NLOBE * tk)
    return e

def polar_samples(I, cx, cy, rlo, rhi, step=0.125):
    th = np.radians(np.arange(0, 360, 0.5)); r = np.arange(rlo, rhi, step)
    T, R = np.meshgrid(th, r, indexing='ij')
    X = cx + R * np.sin(T); Y = cy - R * np.cos(T)
    return T.ravel(), R.ravel(), bil(I, X, Y).reshape(-1, 3), X.ravel(), Y.ravel()

def fit_bins(T, TK, coord, V, lo, hi, width, mask, still_below=-1.0, extra=False):
    # one coefficient set per bin centre (bins at lo, lo + width, ...), from the samples within +-width; bins below
    # still_below get no flute terms (the cap and the inner body do not change as the knob turns)
    out = []
    centres = np.arange(lo, hi + 1e-6, width)
    for c in centres:
        m = mask & (np.abs(coord - c) < width)
        nb = NB + (4 if extra else 0)
        if m.sum() < 60: out.append(np.zeros((nb, 3))); continue
        X = basis(T[m], TK[m], extra)
        if c < still_below:
            co = np.zeros((nb, 3)); co[:9] = lsq(X[:, :9], V[m]); out.append(co)
        else: out.append(lsq(X, V[m]))
    return centres, np.array(out)

def fit_kind(name, frames, median, cx, cy, rs, rbody, flutes, shadow_out, rfin, panel=panel, fo=(0.0, 0.0), co=(0.0, 0.0),
             extra=False, rimbins=8, print_below=0.42):
    # frames: (image, pointer angle in radians, JUCE convention[, flute turn]); the flutes turn with the pointer from
    # angle 0. (cx, cy): the skirt's (the base's) centre; fo, co: the flute ring's and the cap's centres from it (a raised
    # knob seen a little from above: its top lies lower right of its base)
    parts = [polar_samples(f[0], cx, cy, 0.0, rs + 0.5) for f in frames]
    T = np.concatenate([p[0] for p in parts]); R = np.concatenate([p[1] for p in parts]); V = np.concatenate([p[2] for p in parts])
    X = np.concatenate([p[3] for p in parts]); Y = np.concatenate([p[4] for p in parts])
    K = np.concatenate([np.full(len(p[0]), turn(f)) for p, f in zip(parts, frames)])
    def polar(dx, dy):
        if dx == 0.0 and dy == 0.0: return T, R               # concentric (H-Comp's knobs): the sampling grid itself
        x = X - cx - dx; y = Y - cy - dy
        return np.mod(np.arctan2(x, -y), 2 * np.pi), np.hypot(x, y)
    Tf, Rf = polar(*fo); Tc, Rc = polar(*co)
    free = ~np.concatenate([pointer_mask(p[0], p[1], f[1], rfin) for p, f in zip(parts, frames)])
    res = {'name': name, 'rs': rs, 'fo': fo, 'co': co}
    fx, fy = cx + fo[0], cy + fo[1]
    ec = edge_fit(frames, fx, fy, rbody - 6, rbody + 4, rfin) if flutes else edge_fit(frames, fx, fy, rs - 3.5, rs + 2.5, rfin)
    res['edge'] = ec
    E = edge_eval(ec, Tf - K)
    emin = ec[0] - np.abs(ec[3:]).sum()
    res['emin'] = emin
    # inner body (about the cap's centre): absolute radius, 0.5 px bins, up to emin - 0.5
    res['inner'] = fit_bins(Tc, Tc - K, Rc, V, 0.0, emin - 0.5, 0.5, free & (Rc < emin) & (Rf < E), still_below=emin - STILL, extra=extra)
    # rim band (about the flutes' centre): normalised from emin - 2 to the edge
    t = (Rf - (emin - 2.0)) / (E - (emin - 2.0))
    res['rim'] = fit_bins(Tf, Tf - K, t, V, 0.0, 1.0, 1.0 / rimbins, free & (t >= -0.1) & (t <= 1.0) & (Rf < E), extra=extra)
    if flutes:
        res['skirt'] = fit_bins(T, T - K, R, V, emin - 2.0, rs, 0.5, free & (Rf > E) & (R < rs + 0.25), extra=extra)
    # shadow: the panel darkened by a soft disc offset down and to the right (fitted outside the knob, print excluded)
    I = median
    x0, y0 = int(cx - rs - 26), int(cy - rs - 26)
    sub = I[y0:y0 + 2 * int(rs + 26), x0:x0 + 2 * int(rs + 26)]
    ys, xs = np.mgrid[0:sub.shape[0], 0:sub.shape[1]]; Xs = xs + x0 + 0.5; Ys = ys + y0 + 0.5
    ratio = sub / panel(Xs, Ys); Lr = ratio.mean(axis=2)
    rr = np.hypot(Xs - cx, Ys - cy)
    m = (rr > rs + 1.0) & ~ndimage.binary_dilation(Lr < print_below, iterations=1) & (rr < rs + 25) & (Ys > cy - 10)
    D = 1.0 - Lr
    def model(p):
        dx, dy, Rs, w, s = p
        return s * 0.5 * (1.0 - np.tanh((np.hypot(Xs - cx - dx, Ys - cy - dy) - Rs) / w))
    best = optimize.minimize(lambda p: ((model(p)[m] - D[m]) ** 2).sum(), [6, 9, rs, 4, 0.5], method='Nelder-Mead',
                             options={'maxiter': 4000, 'xatol': 1e-3, 'fatol': 1e-6})
    res['shadow'] = best.x
    return res

def hdelay_kinds():
    # H-Delay (reference/hdelay/knobs): Feedback, Mix and Output typed to nine values in turn, then a click on the panel
    # (f_{4i+4}: all three unselected); the values each frame shows are read back from the capture's log
    KH = 'reference/hdelay/knobs'
    vals = []
    for line in open(KH + '/f.log', encoding='utf-8', errors='replace'):
        if line.startswith('after:'):
            v = dict(re.findall(r'\[([^=\]]+)=([^\]]*)\]', line))
            vals.append(v)
    jr = lambda deg: math.radians((deg + 180.0) % 360.0)
    angle = lambda frac: jr(39 + 282 * frac)                                   # the scales: 21 ticks of 14.1 degrees
    grand, mini, frames = [], [], []
    for i in range(9):
        k = 4 * i + 4
        I = np.asarray(Image.open('%s/f_%d_after.png' % (KH, k)).convert('RGB')).astype(float)
        v = vals[k - 1]
        fb = float(v['Feedback']); mix = float(v['Mix']); out = float(v['Output'].split()[0])
        frames.append(I)
        grand.append((I, angle(fb / 200.0)))
        mini.append((I, angle(mix / 100.0)))
        mini.append((I, angle((out + 18.0) / 36.0), 'out'))
    med = np.median(np.stack(frames), axis=0)
    # Mix and Output are the same knob: the Output frames are moved onto Mix's centre
    dy = 142.0 - 62.07
    mini2 = [(f[0], f[1]) if len(f) == 2 else (np.roll(f[0], -int(round(dy)), axis=0), f[1]) for f in mini]
    # the filmstrip's flutes turn in coarse steps (up to 3 degrees off the pointer's angle): each frame's own flute turn,
    # from the phase of its edge's 10-lobe harmonic
    # centres (fitted on the median of the captures): the skirt's outer edge (516.03, 101.51); the flute ring 1.08 px
    # right of it; the cap's edge 1.07 right and 2.27 down
    cx, cy = 517.11, 101.55
    th = np.radians(np.arange(0, 360, 0.5)); r = np.arange(40, 56, 0.1)
    d = []
    for I, J in grand:
        L = I.mean(axis=2)
        T, R = np.meshgrid(th, r, indexing='ij'); P = bil(L, cx + R * np.sin(T), cy - R * np.cos(T))
        e = r[np.argmax(np.diff(P, axis=1), axis=1)]
        z = np.sum((e - e.mean()) * np.exp(-1j * NLOBE * th))
        d.append(np.angle(np.exp(1j * (-np.angle(z) - NLOBE * J))))
    d = np.array(d); dm = np.angle(np.mean(np.exp(1j * d)))
    grand = [(I, J, J + wrap(dk - dm) / NLOBE) for (I, J), dk in zip(grand, d)]
    print('Grand flute turn off the pointer (deg):', np.round(np.degrees([f[2] - f[1] for f in grand]), 2))
    # the small knobs (Depth, Rate, HiPass, LoPass typed to nine values in turn, then a click on the panel: s_{5i+5}; not
    # every entry took, so each frame's values are read back), all moved onto Depth's centre
    svals = [dict(re.findall(r'\[([^=\]]+)=([^\]]*)\]', l)) for l in open(KH + '/s.log', encoding='utf-8', errors='replace')
             if l.startswith('after:')]
    cen = {'Depth': (181.42, 223.15), 'Rate': (286.16, 222.61), 'HiPass': (407.16, 222.61), 'LoPass': (542.25, 222.56)}
    def frac(name, text):
        if name == 'Depth': return float(text.split()[0]) / 100.0
        if name == 'Rate':
            v = float(text.split()[0].replace('k', '')) * (1000.0 if 'k' in text else 1.0)
            return math.log(v / 0.1) / math.log(60000.0)
        if text.strip() == 'Off': return 0.0 if name == 'HiPass' else 1.0
        v = float(text.replace('kHz', '').replace('Hz', '')) * (1000.0 if 'kHz' in text else 1.0)
        return math.log(v / 19.0) / math.log(20138.77 / 19.0)
    small, smed = [], []
    for i in range(9):
        k = 5 * i + 5
        I = np.asarray(Image.open('%s/s_%d_after.png' % (KH, k)).convert('RGB')).astype(float)
        v = svals[k - 1]
        smed.append(I)
        for name, (x, y) in cen.items():
            dx, dy = cen['Depth'][0] - x, cen['Depth'][1] - y
            moved = I if name == 'Depth' else ndimage.shift(I, (dy, dx, 0), order=1, mode='nearest')
            small.append((moved, angle(frac(name, v[name]))))
    smed = np.median(np.stack(smed), axis=0)
    return [fit_kind('Grand', grand, med, 516.03, 101.51, 63.4, 47.0, True, 22, 46.0, panel=panel_hd, fo=(1.08, 0.04), co=(1.07, 2.27),
                     extra=True, rimbins=16, print_below=0.3),
            fit_kind('Knurled', small, smed, 181.42, 223.15, 32.4, 32.4, False, 24, 26.0, panel=panel_hd, print_below=0.3),
            fit_kind('Mini', mini2, med, 698.69, 62.07, 23.65, 23.65, False, 18, 0.0, panel=panel_hd, print_below=0.3)]

def main():
    # the captures: after typing step i (of 8) into Punch, Ratio, Threshold in turn (f_{3i+1} .. f_{3i+3}).
    # f_{3i+3}: Punch and Ratio at step i (Ratio's steps 7, 8 did not take); f_{3i+1}: Threshold still at step i - 1,
    # unselected (its first entry did not take: -18 dB)
    F3 = {i: load(3 * i + 3) for i in range(9)}
    F1 = {i: load(3 * i + 1) for i in range(1, 8)}
    jr = lambda deg: math.radians((deg + 180.0) % 360.0)                          # reference angle -> JUCE
    small = [(F3[i], jr(39 + 282 * i / 8)) for i in range(9)]
    large = [(F3[i], jr(40 + 280 * i / 8)) for i in range(7)]
    huge = [(F1[i], jr(53 + 254 * (0.375 if i == 1 else (i - 1) / 8))) for i in range(1, 8)]
    A = np.median(np.stack([F3[i] for i in range(7)]), axis=0)                    # pointers removed: the cast shadows
    B = np.median(np.stack(list(F1.values())), axis=0)
    kinds = [fit_kind('Small', small, A, 58.25, 344.08, 32.4, 32.4, False, 24, 26.0),
             fit_kind('Large', large, A, 86.07, 218.47, 53.9, 44.0, True, 22, 39.0),
             fit_kind('Huge', huge, B, 320.65, 333.35, 73.0, 59.5, True, 22, 55.0)]
    kinds += hdelay_kinds()
    q = 8.0                                                                     # stored as int16 * 1/8
    lines = ['#pragma once', '// Generated by tools/fit_hybrid_knobs.py from measurements of the reference knobs. Do not edit.',
             '// Per radial bin: ' + str(NB) + ' angular basis coefficients (1, cos/sin 1..3, highlight lobes 11 and 28 deg at 320 deg,',
             '// cos/sin of the 10 flutes, and those under a 40 deg window at the highlight) for R, G and B, stored x8.',
             '// Light terms take the angle on screen, flute terms (and the edge) the angle on the knob turned by the pointer.',
             '// Shadow: the panel darkened by s * (1 - tanh((|p - c - (dx, dy)| - r) / w)) / 2.',
             '// fdx, fdy / cdx, cdy: the flute ring\'s and the cap\'s centres from the skirt\'s (the inner and rim tables take them).',
             '#include <cstdint>', 'namespace wavy::hybrid::knobdata', '{',
             'constexpr int kBasis = %d, kLobes = %d;' % (NB, NLOBE), 'constexpr float kLight = %.4ff;' % LIGHT,
             'struct Table { float lo, step; int n; const int16_t* c; int nb; };   // nb: basis terms (13, or 17 with the flutes\' harmonics)',
             'struct Shadow { float dx, dy, r, w, s; };',
             'struct Kind { float rs, emin; float edge[9]; Table inner, rim, skirt; Shadow shadow; float fdx, fdy, cdx, cdy; };']
    def arr(nm, a, scale):
        flat = np.clip(np.round(a.ravel() * scale), -32767, 32767).astype(int)
        body = ','.join(str(v) for v in flat)
        lines.append('inline constexpr int16_t %s[] = {%s};' % (nm, body))
    for k in kinds:
        n = k['name']
        for part in ('inner', 'rim', 'skirt'):
            if part in k:
                c, a = k[part]
                arr('k%s_%s' % (n, part), a.transpose(0, 2, 1), q)      # [bin][channel][basis]
    for k in kinds:
        n = k['name']
        def tab(part):
            if part not in k: return '{ 0.0f, 1.0f, 0, nullptr, %d }' % NB
            c, a = k[part]
            return '{ %.4ff, %.4ff, %d, k%s_%s, %d }' % (c[0], c[1] - c[0], len(c), n, part, a.shape[1])
        lines.append('inline constexpr Kind k%s { %.3ff, %.3ff, { %s }, %s, %s, %s, { %s }, %.2ff, %.2ff, %.2ff, %.2ff };' % (
            n, k['rs'], k['emin'], ', '.join('%.4ff' % v for v in k['edge']), tab('inner'), tab('rim'), tab('skirt'),
            ', '.join('%.3ff' % v for v in k['shadow']), k['fo'][0], k['fo'][1], k['co'][0], k['co'][1]))
    lines.append('}')
    with open('Source/common/WavyHybridKnobData.h', 'w', newline='\n') as f: f.write('\n'.join(lines) + '\n')
    for k in kinds: print(k['name'], 'edge', np.round(k['edge'], 2), 'emin %.2f' % k['emin'], 'shadow', np.round(k['shadow'], 2))

if __name__ == '__main__':
    main()
