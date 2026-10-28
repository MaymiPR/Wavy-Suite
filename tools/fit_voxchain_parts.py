"""Fits compact models of the reference CLA Vocals editor (its skin, extracted into the git-ignored
reference/wvoxchain/skin/) and writes plugins/WVoxChain/WVoxChainSkinData.h. No image is copied, only small measured
models that WVoxChainEditor.cpp renders (the methods of tools/fit_voxrider_parts.py):

- the panels (the 1x background, 738 x 474 with the toolbar's strip): a colour per row times a factor per column times
  a coarse correction grid, and the grain left over, the printed art masked; the grille as a measured hole pattern;
- the printed art as measured items (labels: string, ink box, grey; the scale's dashes as runs);
- the small parts (fader track, fader caps and shadow, meter frames, the colour windows and buttons) as low-rank
  models; the round parts (screws, the Balance knob, the Sensitivity LED) as polar models round their centres.

The engineer's signature plate, the maker's mark and the product badge are left out (the panel model fills them;
WVox Chain prints its own name plate).

    python -I tools/fit_voxchain_parts.py [--preview]
"""
import math
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fit_voxrider_parts as F                  # (lowrank, polar, panel, components and their emitters)

ROOT = os.path.join(HERE, '..')
SKIN = os.path.join(ROOT, 'reference', 'wvoxchain', 'skin', 'PNG_')
OUT = os.path.join(ROOT, 'plugins', 'WVoxChain', 'WVoxChainSkinData.h')
PREVIEW = os.path.join(ROOT, 'reference', 'wvoxchain', 'shots', 'fit_preview.png')
W, H = 738, 474


def load(n):
    """a skin image (1x) as premultiplied RGBA 0 .. 1 (h, w, 4)"""
    a = np.asarray(Image.open(os.path.join(SKIN, '%d.png' % n)).convert('RGBA')).astype(float) / 255.0
    return np.concatenate([a[..., :3] * a[..., 3:4], a[..., 3:4]], 2)


def base():
    return np.asarray(Image.open(os.path.join(SKIN, '15000.png')).convert('RGB')).astype(float)


# the faders: control rectangle x (34 wide, 209 tall from y 210); the track centre is 17 px in
FADERS = [('sensitivity', 49), ('bass', 186), ('treble', 253), ('compress', 319), ('reverb', 386), ('delay', 452), ('pitch', 519), ('output', 655)]

# panels: name, y0, y1, x0, x1, correction cell, art boxes (x0, y0, x1, y1) the models leave out
def panel_list():
    tracks = [(x - 1, 190, x + 35, 430) for _, x in FADERS]
    # the scale's dashes and numbers between the tracks (three rows)
    scales = [(a, y, b, y + 12) for y in (233, 308, 384) for a, b in ((22, 48), (84, 104), (150, 186), (220, 253), (287, 319), (353, 386), (420, 452), (486, 519), (553, 577), (637, 655), (689, 716))]
    meters = [(103, 188, 147, 432), (593, 188, 637, 432)]
    labels = [(60, 429, 125, 443), (185, 429, 220, 443), (248, 429, 292, 443), (309, 429, 365, 443), (382, 429, 427, 443),
              (450, 429, 490, 443), (518, 429, 556, 443), (648, 429, 698, 443)]
    return [
        ('header', 19, 107, 22, 716, 8, [(30, 70, 50, 90), (686, 70, 706, 90), (55, 62, 210, 98), (655, 30, 700, 50)]),
        ('displayFrame', 107, 183, 164, 577, 8, [(170, 111, 570, 159)]),
        ('displayWindow', 112, 157, 172, 568, 4, []),
        ('badge', 107, 183, 577, 716, 8, [(600, 125, 695, 165)]),
        ('lowerLeft', 181, 474, 22, 164, 6, tracks[:1] + meters[:1] + labels[:1] + scales),
        ('lowerMid', 181, 474, 164, 577, 6, tracks[1:7] + labels[1:7] + scales),
        ('lowerRight', 181, 474, 577, 716, 6, tracks[7:] + meters[1:] + labels[7:] + scales),
        ('railLeft', 19, 474, 0, 22, 8, []),
        ('railRight', 19, 474, 716, 738, 8, []),
    ]


def panel(rgb, name, y0, y1, x0, x1, FX, art=()):
    """F.panel's model (a colour per row times a factor per column, a correction over FX px cells, the grain), with the
    rows and columns that the art covers entirely taken from their neighbours (linear between) rather than from the art"""
    reg = rgb[y0:y1, x0:x1]
    g = reg.mean(2)
    known = np.zeros(g.shape, bool)
    for a, b, c, e in art: known[max(int(b) - y0, 0):max(int(math.ceil(e)) - y0, 0), max(int(a) - x0, 0):max(int(math.ceil(c)) - x0, 0)] = True
    m = (np.abs(g / np.maximum(np.median(g, 1)[:, None], 1) - 1) < 0.15) & ~known
    def fill(v, ok):
        idx = np.arange(len(v))
        if ok.sum() == 0: return v
        return np.stack([np.interp(idx, idx[ok], v[ok, c]) for c in range(v.shape[1])], 1) if v.ndim == 2 else np.interp(idx, idx[ok], v[ok])
    for _ in range(4):
        rok = m.sum(1) > max(4, 0.3 * m.shape[1])
        rows = np.array([np.median(reg[i][m[i]], 0) if rok[i] else np.zeros(3) for i in range(len(reg))])
        rows = fill(rows, rok)
        lum = g / np.maximum(rows.mean(1)[:, None], 1)
        cok = m.sum(0) > max(4, 0.3 * m.shape[0])
        cols = np.array([np.median(lum[:, j][m[:, j]]) if cok[j] else 1.0 for j in range(g.shape[1])])
        cols = fill(cols, cok)
        model = rows.mean(1)[:, None] * cols[None, :]
        res = g - model
        sd = 1.4826 * np.median(np.abs(res[m])) if m.any() else 1.0
        m = (np.abs(res) < max(4.0 * sd, 22.0)) & ~known
    gys = np.arange(FX / 2, y1 - y0, FX); gxs = np.arange(FX / 2, x1 - x0, FX)
    fix = np.full((len(gys), len(gxs)), np.nan)
    ratio = g / np.maximum(model, 1)
    for j, gy in enumerate(gys):
        for i, gx in enumerate(gxs):
            sl = (slice(int(gy - FX / 2), int(gy + FX / 2)), slice(int(gx - FX / 2), int(gx + FX / 2)))
            if m[sl].sum() > FX * FX / 4: fix[j, i] = np.median(ratio[sl][m[sl]])
    # cells under the art: the mean of their known neighbours, repeatedly (smooth fill)
    for _ in range(200):
        bad = np.isnan(fix)
        if not bad.any(): break
        pad = np.pad(fix, 1, constant_values=np.nan)
        nb = np.stack([pad[:-2, 1:-1], pad[2:, 1:-1], pad[1:-1, :-2], pad[1:-1, 2:]])
        cnt = (~np.isnan(nb)).sum(0)
        avg = np.where(cnt > 0, np.nansum(nb, 0) / np.maximum(cnt, 1), np.nan)
        fix = np.where(bad & (cnt > 0), avg, fix)
    fix = np.clip(np.nan_to_num(fix, nan=1.0), 0.5, 1.5)
    yy, xx = np.mgrid[0:g.shape[0], 0:g.shape[1]]
    fx = np.clip((xx + 0.5 - gxs[0]) / FX, 0, len(gxs) - 1.001); fy = np.clip((yy + 0.5 - gys[0]) / FX, 0, len(gys) - 1.001)
    i, j = fx.astype(int), fy.astype(int); tx, ty = fx - i, fy - j
    Fm = (fix[j, i] * (1 - tx) + fix[j, i + 1] * tx) * (1 - ty) + (fix[j + 1, i] * (1 - tx) + fix[j + 1, i + 1] * tx) * ty
    res = (reg - rows[:, None, :] * cols[None, :, None] * Fm[..., None]).mean(2)
    grain = float(np.sqrt(np.mean(np.clip(res[m], -40, 40) ** 2)))
    print('%-14s %d x %d, art %.1f %%, grain rms %.1f' % (name, len(cols), len(rows), 100 * (~m).mean(), grain))
    return dict(name=name, y0=y0, x0=x0, rows=rows, cols=cols, fix=fix, fx0=gxs[0] + x0, fy0=gys[0] + y0, step=FX, grain=grain,
                model=rows[:, None, :] * cols[None, :, None] * Fm[..., None])


def disc(rgb, cx, cy, r):
    """the pixels round (cx, cy) as premultiplied RGBA, opaque to r - 0.5 and fading out over the next pixel"""
    x0, y0 = int(math.floor(cx - r - 1)), int(math.floor(cy - r - 1)); n = int(2 * r + 3)
    P = rgb[y0:y0 + n, x0:x0 + n] / 255.0
    yy, xx = np.mgrid[0:n, 0:n]
    al = np.clip(r + 0.5 - np.hypot(xx + 0.5 - (cx - x0), yy + 0.5 - (cy - y0)), 0, 1)[..., None]
    return np.concatenate([P * al, al], 2), x0, y0


def overlay(rgb, model, boxes, name, k=4, holes=()):
    """the art over a panel that repeats at several places (boxes of the same size): the mean difference to the panel
    model, as a low-rank layer of signed RGB deltas (0 .. 1); holes (x, y, r relative to a box) are left out"""
    w, h = boxes[0][2] - boxes[0][0], boxes[0][3] - boxes[0][1]
    D = np.mean([(rgb[y0:y1, x0:x1] - model[y0:y1, x0:x1]) / 255.0 for x0, y0, x1, y1 in boxes], 0)
    P = np.concatenate([D, np.zeros((h, w, 1))], 2)
    yy, xx = np.mgrid[0:h, 0:w]
    mask = np.zeros((h, w), bool)
    for hole in holes:
        if hole[0] == 'c': mask |= np.hypot(xx + 0.5 - hole[1], yy + 0.5 - hole[2]) < hole[3]
        else: mask |= (xx >= hole[1]) & (yy >= hole[2]) & (xx < hole[3]) & (yy < hole[4])
    m = F.lowrank(P, k, mask if mask.any() else None)
    rec = np.einsum('yjc,xjc->yxc', m['u'].reshape(h, k, 4), m['v'].reshape(w, k, 4))[..., :3]
    print('%-14s %d x %d rank %d, rms %.2f (of %.2f)' % (name, w, h, k, 255 * np.sqrt(np.mean((rec - D)[~mask] ** 2)), 255 * np.sqrt(np.mean(D[~mask] ** 2))))
    return m, rec


def render_polar(m, cx, cy, rmax, n):
    """a polar model drawn at (cx, cy) (absolute): premultiplied RGBA (n x n) and its top-left"""
    x0, y0 = int(math.floor(cx - n / 2)), int(math.floor(cy - n / 2))
    nb = 1 + 2 * m['nh']; C = m['c']
    img = np.zeros((n, n, 4))
    for yi in range(n):
        for xi in range(n):
            dx, dy = x0 + xi + 0.5 - cx, y0 + yi + 0.5 - cy
            r = math.hypot(dx, dy)
            if r > rmax: continue
            th = math.atan2(dx, -dy)
            bb = np.array([1.0] + sum(([math.cos(k * th), math.sin(k * th)] for k in range(1, m['nh'] + 1)), []))
            fi = min(r / m['step'], m['radii'] - 1 - 1e-3); i = int(fi); t = fi - i
            img[yi, xi] = bb @ C[i] * (1 - t) + bb @ C[i + 1] * t
    return img, x0, y0


def over(dst, img, x0, y0):
    h, w = img.shape[:2]
    a = np.clip(img[..., 3:4], 0, 1)
    dst[y0:y0 + h, x0:x0 + w] = dst[y0:y0 + h, x0:x0 + w] * (1 - a) + 255 * np.clip(img[..., :3], 0, 1)
    return dst


def grille(rgb):
    """the grille (x 22 .. 164, y 107 .. 181): a hexagonal lattice of round holes; the lattice found by the best fit
    of a dark-disc template, the bars a panel model with the holes left out, one hole shape (polar, its colour per
    radius and angle) averaged over the holes, and each hole's tint (its mean colour against the average)"""
    x0, y0, x1, y1 = 22, 107, 164, 181
    g = rgb[y0:y1, x0:x1].mean(2)
    yy, xx = np.mgrid[0:y1 - y0, 0:x1 - x0]
    best = None
    for a in np.arange(10.9, 11.31, 0.1):
        dy = a * math.sqrt(3) / 2
        for ox in np.arange(0, a, 0.5):
            for oy in np.arange(0, dy, 0.5):
                inner = np.zeros(g.shape, bool); ring = np.zeros(g.shape, bool)
                for j in range(-1, int((y1 - y0) / dy) + 2):
                    for i in range(-1, int((x1 - x0) / a) + 2):
                        cx, cy = ox + (j % 2) * a / 2 + i * a, oy + j * dy
                        if not (7 <= cx <= x1 - x0 - 7 and 7 <= cy <= y1 - y0 - 7): continue
                        sl = (slice(int(cy) - 6, int(cy) + 7), slice(int(cx) - 6, int(cx) + 7))
                        r = np.hypot(xx[sl] + 0.5 - cx, yy[sl] + 0.5 - cy)
                        inner[sl] |= r < 3.0; ring[sl] |= (r > 4.6) & (r < 5.6)
                score = g[ring].mean() - g[inner].mean()       # (the holes' middles darker than the bars round them)
                if best is None or score > best[0]: best = (score, a, ox, oy)
    _, a, ox, oy = best
    dy = a * math.sqrt(3) / 2
    centres = [(x0 + ox + (j % 2) * a / 2 + i * a, y0 + oy + j * dy) for j in range(-1, int((y1 - y0) / dy) + 2)
               for i in range(-1, int((x1 - x0) / a) + 2)]
    centres = [(cx, cy) for cx, cy in centres if x0 + 7 <= cx <= x1 - 7 and y0 + 7 <= cy <= y1 - 7]
    # the holes' darkest part sits above their middle (lit from below): their centres, measured on the zoomed skin, are
    # 0.75 px right and 2 px below the darkness fit's
    from scipy import ndimage as nd
    gs = nd.uniform_filter(rgb.mean(2), 3)
    offs = []
    for cx, cy in centres:
        ys, xs = np.mgrid[int(cy) - 5:int(cy) + 6, int(cx) - 5:int(cx) + 6]
        r = np.hypot(xs + 0.5 - cx, ys + 0.5 - cy); v = gs[ys, xs]
        k = (r < 5) & (v <= np.percentile(v[r < 5], 30))
        offs.append((xs[k].mean() + 0.5 - cx, ys[k].mean() + 0.5 - cy))
    mx, my = np.mean(offs, 0) + np.array([0.75, 2.0])
    centres = [(cx + mx, cy + my) for cx, cy in centres]
    ox, oy = ox + mx, oy + my
    R = 4.5
    print('grille         lattice %.2f px, origin (%.2f, %.2f) (moved %.2f, %.2f), hole edge r %.2f, %d holes' % (a, ox, oy, mx, my, R, len(centres)))
    return dict(a=a, ox=x0 + ox, oy=y0 + oy, centres=centres, r=R)


def part(lines, name, P, k, mask=None):
    m = F.lowrank(P, k, mask)
    h, w = P.shape[:2]
    rec = np.einsum('yjc,xjc->yxc', m['u'].reshape(h, k, 4), m['v'].reshape(w, k, 4))
    ok = np.ones((h, w), bool) if mask is None else ~mask
    print('%-14s %d x %d rank %d, rms %.2f' % (name, w, h, k, 255 * np.sqrt(np.mean((rec - P)[ok] ** 2))))
    F.emit_lowrank(lines, name, m)


def printing(lines, rgb):
    """the printed text as labels (string, ink box, grey, bold, italic: the editor fits the font's outline into the box)
    and the scale's loose dashes as runs; the strings are given, the boxes measured"""
    g = rgb.mean(2)
    labels, runs = [], []
    def comps(reg, thr):
        return [c for c in F.components(g, reg, True, thr) if c['n'] > 3]
    # the channel labels under the faders (bold)
    names = {66: 'INPUT SENS', 191: 'BASS', 251: 'TREBLE', 313: 'COMPRESS', 386: 'REVERB', 455: 'DELAY', 522: 'PITCH', 627: 'OUTPUT'}
    for c in comps((22, 426, 716, 445), 25):
        if c['y1'] - c['y0'] >= 5 and c['x1'] - c['x0'] >= 15:
            t = min(names, key=lambda k: abs(k - c['x0']))
            labels.append((names[t], c['x0'], c['y0'], c['x1'], c['y1'], c['grey'], True, False))
    # the meters' numbers (bold italic, light)
    for x0, x1 in ((105, 145), (595, 635)):
        for c in comps((x0 + 8, 222, x1 - 8, 395), 30):
            t = {227: '0', 269: '6', 305: '12', 340: '18', 376: '24'}[min((227, 269, 305, 340, 376), key=lambda k: abs(k - c['y0']))]
            labels.append((t, c['x0'], c['y0'], c['x1'], c['y1'], c['grey'], True, True))
    # the scales (italic): per row the numbers with their dashes where they touch, the loose dashes as runs
    for y0, y1, num in ((230, 248, '+10'), (305, 323, '0'), (381, 399, '-10')):
        for c in comps((22, y0, 716, y1), 12):
            w, h = c['x1'] - c['x0'], c['y1'] - c['y0']
            if c['x0'] in range(99, 170) or c['x0'] in range(570, 640) or w <= 2: continue   # (the meters' edges, the panels' seams, the tracks' rims)
            if h <= 2: runs.append((c['x0'], c['y0'], w, h, c['grey'])); continue
            if c['x1'] <= 60: t = num + ' –'                        # left of Sensitivity: the number, then a dash
            elif c['x0'] >= 680: t = '– ' + num                       # right of Output
            elif w >= 26: t = '– ' + num + ' –'                  # between the faders, joined to both dashes
            elif w >= 12: t = num + ' –'                              # joined to the right-hand dash
            else: t = num
            labels.append((t, c['x0'], c['y0'], c['x1'], c['y1'], c['grey'], False, True))
    print('printing       %d labels, %d dash runs' % (len(labels), len(runs)))
    lines.append('inline constexpr Label labels[] = {')
    lines += ['    { "%s", %.1ff, %.1ff, %.1ff, %.1ff, %.1ff, %s, %s },' % (t.encode('ascii', 'xmlcharrefreplace').decode() if False else t.replace('–', '\\xe2\\x80\\x93'),
              a, b, c_, d, gr, 'true' if bo else 'false', 'true' if it else 'false') for t, a, b, c_, d, gr, bo, it in labels]
    lines.append('};')
    lines.append('inline constexpr Run runs[] = { %s };' % ', '.join('{ %d.0f, %d.0f, %d.0f, %d.0f, %.1ff }' % r for r in runs))


def controls(lines, rgb=None):
    """the controls' parts from the skin's control images (and the side rails from the background)"""
    if rgb is not None:
        # the chrome rails left and right (22 px wide, from the toolbar's foot down): opaque low-rank parts (the rows
        # and columns vary too much for the panel model, whose columns' medians treat the highlights as art)
        for name, x0 in (('railLeftPart', 0), ('railRightPart', 716)):
            P = np.concatenate([rgb[19:474, x0:x0 + 22] / 255.0, np.ones((455, 22, 1))], 2)
            part(lines, name, P, 3)
    # fader caps (34 x 58; Sensitivity's own) and their shadow (43 x 64, drawn at the control's top-left)
    part(lines, 'cap', load(1000), 6)
    part(lines, 'capSens', load(999), 6)
    part(lines, 'capShadow', load(1005), 4)
    # the colour windows (55 x 81): the flavour window above (its names masked: the editor prints them) and the push
    # button below, unlit (colour off) and lit; the 1 px ring in the button is where the selection frame is drawn
    off, on = load(600), load(603)
    tm = np.zeros(off.shape[:2], bool); tm[1:39, 5:50] = True
    part(lines, 'colourWindow', off[:42], 6, tm[:42])
    part(lines, 'buttonOff', off[42:], 6)
    part(lines, 'buttonOn', on[42:], 6)
    # the Sensitivity LED (28 x 25), 24 frames from off through green and yellow to red
    for i in range(24): part(lines, 'sensLed%d' % i, load(13050 + i), 3)
    lines.append('inline constexpr const LowRank* sensLeds[] = { %s };' % ', '.join('&sensLed%d' % i for i in range(24)))
    # the meters (stereo: 9 x 155 per channel, unlit and lit; Mono/Stereo: one 31 x 155 bar) and the clip lights (9 x 5,
    # 31 x 5; off, on)
    for name, n in (('meterOff', 2000), ('meterOn', 2001), ('meterMonoOff', 2020), ('meterMonoOn', 2021),
                    ('clipOff', 2008), ('clipOn', 2009), ('clipMonoOff', 2022), ('clipMonoOn', 2023)):
        part(lines, name, load(n), 4)
    # the Balance knob (30 x 30, 71 frames from -6 to +6): polar round its centre with the pointer, from the middle frame
    P = load(3035)
    cx, cy, r = F.circle(P)
    km = F.polar(P, cx, cy, 15.0, NH=8, step=0.5, pointer=dict(half=1.5, r1=r - 1.0, angle=0.0))
    print('balanceKnob    polar rms %.2f, centre (%.2f, %.2f) r %.2f' % (km['err'], cx, cy, r))
    F.emit_polar(lines, 'balanceKnob', km)
    # the pointer's sweep: the first and last frames' pointer angles (the darkest-brightest ray)
    def pointer_angle(n):
        Q = load(n); best = None
        for a in np.arange(-180, 180, 0.5):
            t = math.radians(a)
            v = np.mean([F.bilinear(Q, cx + rr * math.sin(t), cy - rr * math.cos(t))[:3].sum() for rr in np.arange(4, r - 2, 1.0)])
            if best is None or v > best[0]: best = (v, a)
        return best[1]
    a0, a1 = pointer_angle(3000), pointer_angle(3070)
    print('balance sweep  %.1f .. %.1f deg' % (a0, a1))
    lines.append('inline constexpr float balanceFrom = %.1ff, balanceTo = %.1ff;   // pointer angles at -6 / +6 dB' % (a0, a1))


def main():
    preview = '--preview' in sys.argv
    rgb = base()
    lines = ['#pragma once',
             '// Generated by tools/fit_voxchain_parts.py: models fitted on the reference CLA Vocals editor\'s skin (no image data;',
             '// the editor\'s 1x scale; premultiplied channels 0 .. 1 unless said otherwise). Do not edit.',
             '#include "WavySkinModels.h"',
             'namespace wvoxchain::skin',
             '{',
             'using namespace wavy::skin;',
             '// a printed label: its text (UTF-8), ink box, grey (0 .. 255), bold, italic; a printed dash: x, y, w, h, grey',
             'struct Label { const char* text; float x0, y0, x1, y1, grey; bool bold, italic; };',
             'struct Run { float x, y, w, h, grey; };']
    out = np.zeros_like(rgb)
    for name, y0, y1, x0, x1, fx, art in panel_list():
        p = panel(rgb, name, y0, y1, x0, x1, fx, art)
        F.emit_panel(lines, p)
        if name == 'displayFrame': out[y0:y1, x0:x1] = p['model']
        else: out[y0:y1, x0:x1] = np.where(out[y0:y1, x0:x1].sum(2, keepdims=True) > 0, out[y0:y1, x0:x1], p['model']) if name != 'displayWindow' else p['model']
    # the overlays (differences to the panels, the same at each place) and the screws (one part everywhere)
    comp = out.copy()
    sc, sx0, sy0 = disc(rgb, 202.5, 210.5, 6.5)
    sm = F.polar(sc, 202.5 - sx0, 210.5 - sy0, 7.0, NH=6, step=0.5)
    print('screw          polar rms %.2f' % sm['err'])
    F.emit_polar(lines, 'screw', sm)
    screws = []
    # the channel faders' tracks: a light strip, the slot, a screw at each end (box from the control x + 2, y 190)
    boxes = [(x + 2, 190, x + 32, 432) for _, x in FADERS[1:7]]
    tm, trec = overlay(rgb, out, boxes, 'track', 4, holes=(('c', 14.5, 20.5, 8.0), ('c', 14.5, 227.5, 8.0)))
    F.emit_lowrank(lines, 'track', tm)
    for b in boxes: comp[b[1]:b[3], b[0]:b[2]] += 255 * trec
    screws += [(b[0] + 14.5, y) for b in boxes for y in (210.5, 417.5)]
    # Sensitivity's and Output's: a taller strip (box from the control x + 2, y 186), a screw at the bottom, Output's at the
    # top too (Sensitivity's track has none there): one part each
    for name, x, top in (('sensTrack', 49, False), ('outTrack', 655, True)):
        b = (x + 2, 186, x + 32, 432)
        hole = (('c', 15.5, 231.5, 8.0),) + ((('c', 15.5, 17.5, 8.0),) if top else ())
        tmx, rec = overlay(rgb, out, [b], name, 4, holes=hole)
        F.emit_lowrank(lines, name, tmx)
        comp[b[1]:b[3], b[0]:b[2]] += 255 * rec
        screws += [(b[0] + 15.5, 417.5)] + ([(b[0] + 15.5, 203.5)] if top else [])
    # the meter windows: tabs with screws above and below, the dotted window, the scale's numbers (printed by the editor)
    boxes = [(101, 186, 149, 424), (591, 186, 639, 424)]
    nums = tuple(('r', 16, y - 5, 32, y + 6) for y in (44, 85, 121, 157, 193))
    mm, mrec = overlay(rgb, out, boxes, 'meterWindow', 6, holes=(('c', 24.5, 17.5, 8.0), ('c', 24.5, 225.5, 8.0)) + nums)
    F.emit_lowrank(lines, 'meterWindow', mm)
    for b in boxes: comp[b[1]:b[3], b[0]:b[2]] += 255 * mrec
    screws += [(b[0] + 24.5, y) for b in boxes for y in (203.5, 411.5)]
    lines.append('inline constexpr int trackDx = 2, trackY = 190, sideTrackY = 186;   // overlay boxes: the control x + 2')
    lines.append('inline constexpr int meterInX = 101, meterOutX = 591, meterY = 186;')
    lines.append('inline constexpr float screws[][2] = { %s };' % ', '.join('{ %.1ff, %.1ff }' % s_ for s_ in screws))
    for cx, cy in screws: comp = over(comp, *render_polar(sm, cx, cy, 7.0, sc.shape[1]))
    # the header's two chrome screws (a larger part): each centre where a coarse polar fit leaves the least error
    def centre(cx0, cy0):
        best = None
        for cx in np.arange(cx0 - 1.5, cx0 + 1.51, 0.25):
            for cy in np.arange(cy0 - 1.5, cy0 + 1.51, 0.25):
                P, x0, y0 = disc(rgb, cx, cy, 8.0)
                e = F.polar(P, cx - x0, cy - y0, 8.5, NH=2, step=0.5)['err']
                if best is None or e < best[0]: best = (e, cx, cy)
        return best[1], best[2]
    hs = [centre(40.0, 82.5), centre(698.0, 82.5)]
    P, x0, y0 = disc(rgb, hs[0][0], hs[0][1], 8.0)
    hm = F.polar(P, hs[0][0] - x0, hs[0][1] - y0, 8.5, NH=8, step=0.5)
    print('header screw   polar rms %.2f at %s' % (hm['err'], hs))
    F.emit_polar(lines, 'headerScrew', hm)
    lines.append('inline constexpr float headerScrews[][2] = { %s };' % ', '.join('{ %.2ff, %.2ff }' % c_ for c_ in hs))
    for cx, cy in hs: comp = over(comp, *render_polar(hm, cx, cy, 8.5, 19))
    out = comp
    # the grille: its bars (a panel with the holes left out), one hole shape and each hole's tint
    gr = grille(rgb)
    holes = [(cx - 5.2, cy - 5.2, cx + 5.2, cy + 5.2) for cx, cy in gr['centres']]
    pg = panel(rgb, 'grilleBars', 107, 181, 22, 164, 4, holes)
    F.emit_panel(lines, pg)
    comp[107:181, 22:164] = pg['model']
    patches, tints = [], []
    for cx, cy in gr['centres']:
        P, px0, py0 = disc(rgb, cx, cy, 5.0)
        t = P[..., :3].sum((0, 1)) / max(P[..., 3].sum(), 1e-6)
        tints.append(t); patches.append((P, px0, py0, cx, cy))
    tints = np.array(tints); mean_t = tints.mean(0)
    # the shape: each hole divided by its tint relative to the mean, the holes averaged on a common grid (sub-pixel
    # centres: bilinear), then a polar model round the middle
    n = patches[0][0].shape[0]; acc = np.zeros((n, n, 4))
    for (P, px0, py0, cx, cy), t in zip(patches, tints):
        Q = P.copy(); Q[..., :3] *= (mean_t / np.maximum(t, 1e-4))
        acc += Q
    acc /= len(patches)
    c0 = n / 2.0
    hm = F.polar(acc, c0, c0, 5.5, NH=6, step=0.5)
    print('grille hole    polar rms %.2f, tints %.3f .. %.3f' % (hm['err'], tints.min(), tints.max()))
    F.emit_polar(lines, 'grilleHole', hm)
    rel = tints / mean_t
    lines.append('inline constexpr float grilleLattice = %.2ff, grilleOx = %.2ff, grilleOy = %.2ff;   // spacing; the first hole (rows alternate by half)' % (gr['a'], gr['ox'], gr['oy']))
    lines.append('inline constexpr float grilleHoles[][5] = { %s };   // x, y, tint r, g, b (times the shape)' % ', '.join(
        '{ %.2ff, %.2ff, %.3ff, %.3ff, %.3ff }' % (cx, cy, r_[0], r_[1], r_[2]) for (cx, cy), r_ in zip(gr['centres'], rel)))
    for (cx, cy), r_ in zip(gr['centres'], rel):
        img, ix0, iy0 = render_polar(hm, cx, cy, 5.5, 13)
        img[..., :3] *= r_
        comp = over(comp, img, ix0, iy0)
    out = comp
    printing(lines, rgb)
    controls(lines, rgb)
    lines += ['}', '']
    open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    print('wrote', OUT, '(%d kB)' % (os.path.getsize(OUT) // 1024))
    if preview:
        Image.fromarray(np.clip(np.concatenate([rgb, out], 1), 0, 255).astype(np.uint8)).save(PREVIEW)
        print('preview', PREVIEW)


if __name__ == '__main__':
    main()
