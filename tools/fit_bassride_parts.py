"""Fits compact shading models of the reference Bass Rider's controls (its skin, extracted into the git-ignored
reference/wbassrider/) and writes plugins/WBassRider/WBassRiderSkinData.h: no image is copied, only small measured
models that WBassRiderEditor.cpp renders:

- each fader cap (Rider, Range, Output) and the Response switch's block as a separable shading: a grey level per row
  times a factor per column (both at the editor's 1x scale), plus the cap's alpha per row / column;
(the knobs are drawn parametrically in WBassRiderEditor.cpp, from the same measurements).

    python -I tools/fit_bassride_parts.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SKIN = os.path.join(ROOT, 'reference', 'wbassrider', 'webp', 'Bass Rider_Webp')
OUT = os.path.join(ROOT, 'plugins', 'WBassRider', 'WBassRiderSkinData.h')


def load(n):
    a = np.asarray(Image.open(os.path.join(SKIN, '%s.webp' % n)).convert('RGBA')).astype(float)
    # 2x -> 1x (premultiplied box filter)
    h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
    a = a[:h, :w]
    al = a[..., 3:4] / 255.0
    pm = a[..., :3] * al
    pm = pm.reshape(h // 2, 2, w // 2, 2, 3).mean((1, 3))
    al = al.reshape(h // 2, 2, w // 2, 2, 1).mean((1, 3))
    rgb = np.where(al > 1e-3, pm / np.maximum(al, 1e-3), 0)
    return rgb, al[..., 0]


def separable(n, box=None):
    """grey per row x factor per column over the opaque part; returns rows, cols, alpha rows/cols, rms error"""
    rgb, al = load(n)
    if box: rgb, al = rgb[box[1]:box[3], box[0]:box[2]], al[box[1]:box[3], box[0]:box[2]]
    g = rgb.mean(2)
    m = al > 0.9
    cov = m.mean(0)
    cols_core = np.where(cov > 0.6 * cov.max())[0]
    mid = cols_core[len(cols_core) // 4: 3 * len(cols_core) // 4]
    rows = np.array([np.median(g[y, mid][m[y, mid]]) if m[y, mid].any() else 0 for y in range(g.shape[0])])
    cols = np.array([np.median(g[m[:, x], x] / np.maximum(rows[m[:, x]], 1)) if m[:, x].any() else 1 for x in range(g.shape[1])])
    model = rows[:, None] * cols[None, :]
    err = np.sqrt(np.mean((model - g)[m] ** 2))
    tint = (rgb[m].mean(0) / max(rgb[m].mean(), 1)).tolist()
    u, sv, vt = np.linalg.svd(al)
    ar, ac = np.abs(u[:, 0]) * np.sqrt(sv[0]), np.abs(vt[0]) * np.sqrt(sv[0])
    k = max(ar.max(), 1e-6); ar, ac = ar / k, ac * k
    aerr = np.sqrt(np.mean((np.outer(ar, ac) - al) ** 2))
    print('   alpha rank-1 rms %.3f' % aerr)
    return rows, cols, ar, ac, err, tint


def knob():
    rgb, al = load('10136')                     # the middle frame: the pointer straight up
    g = rgb.mean(2); h, w = g.shape
    cy, cx = (h - 1) / 2, (w - 1) / 2
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot(yy - cy, xx - cx)
    # away from the pointer (the lower half), the dome: grey by radius, with a linear top-to-bottom tilt
    sel = (al > 0.95) & (yy > cy + 2)
    A = np.stack([np.ones(sel.sum()), (yy[sel] - cy) / cy], 1)
    radial = []
    edges = np.arange(0, cx + 0.5, 0.5)
    for r0 in edges:
        s = sel & (np.abs(r - r0) < 0.5)
        radial.append(g[s].mean() if s.any() else np.nan)
    radial = np.array(radial)
    radial = np.where(np.isnan(radial), np.nanmin(radial), radial)
    # tilt: compare the upper and lower halves at the same radius (excluding the pointer column)
    up = (al > 0.95) & (yy < cy - 2) & (np.abs(xx - cx) > 3)
    lo = (al > 0.95) & (yy > cy + 2)
    tilt = []
    for r0 in edges[4:-4]:
        a = g[up & (np.abs(r - r0) < 0.5)]; b = g[lo & (np.abs(r - r0) < 0.5)]
        if len(a) and len(b): tilt.append((a.mean() - b.mean()) / 2)
    tilt = float(np.mean(tilt))
    # alpha by radius
    alr = np.array([al[np.abs(r - r0) < 0.5].mean() for r0 in edges])
    # the pointer: the dark column above the centre
    colv = g[:int(cy), int(round(cx)) - 2:int(round(cx)) + 3]
    ptr = colv.min(1)
    dark = np.where(ptr < np.median(ptr) - 25)[0]
    p_len0, p_len1 = (cy - dark.max(), cy - dark.min()) if len(dark) else (0, 0)
    p_grey = float(ptr[dark].mean()) if len(dark) else 0
    # pointer width: dark pixels across at mid length
    ym = int(cy - (p_len0 + p_len1) / 2)
    rowp = g[ym, :]
    p_w = float((rowp < (np.median(g[ym, al[ym] > 0.9]) + p_grey) / 2).sum())
    return radial, alr, tilt, (p_len0, p_len1, p_w, p_grey), edges[1] - edges[0]


def panels():
    """the base panel (15000, 2x -> 1x): per region a colour per row (median over the row) and a factor per column
    (median over the region), and the grain left over (rms)"""
    rgb, _ = load('15000')
    out = []
    for name, y0, y1, x0, x1 in (('top', 36, 162, 6, 314), ('cream', 162, 510, 0, 320), ('leather', 510, 560, 0, 320)):
        reg = rgb[y0:y1, x0:x1]
        rows = np.median(reg, 1)                                   # (h, 3)
        lum = reg.mean(2) / np.maximum(rows.mean(1)[:, None], 1)
        cols = np.median(lum, 0)                                   # (w,)
        # keep the shading only: lines that span most of a row or column (the group frames, the tracks, the wells) are
        # drawn by the editor, so rows / columns that stand out of their neighbourhood are replaced by its median
        def clean(v, k, thr, keep=0):
            out = v.copy()
            for i in range(keep, len(v) - keep):                  # (the region's own edges stay)
                med = np.median(v[max(0, i - k): i + k + 1], axis=0)
                if np.max(np.abs(np.atleast_1d(v[i] - med)) / np.maximum(np.atleast_1d(med), 1)) > thr: out[i] = med
            return out
        if name != 'leather':
            rows = clean(rows, 4, 0.06, keep=7)
            cols = clean(cols, 24 if name == 'cream' else 6, 0.02, keep=6 if name == 'cream' else 1)
        model = rows[:, None, :] * cols[None, :, None]
        res = (reg - model).mean(2)
        grain = float(np.sqrt(np.mean(np.clip(res, -40, 40) ** 2)))
        print('%-8s rows %d cols %d grain rms %.1f' % (name, len(rows), len(cols), grain))
        out.append((name, y0, x0, rows, cols, grain))
    return out


def arr(v, fmt='%.1f'):
    return ', '.join((fmt % x) + 'f' for x in v)


def main():
    lines = ['#pragma once', '// Generated by tools/fit_bassride_parts.py: shading models fitted on the reference Bass Rider\'s controls (no image',
             '// data; grey levels 0 .. 255 at the editor\'s 1x scale). Do not edit.', 'namespace wbassrider::skin', '{',
             'struct Cap { int w, h; const float* rows; const float* cols; const float* alphaRows; const float* alphaCols; float tint[3]; };']
    caps = (('rider', '2000', None), ('range', '600', None), ('output', '3000', None), ('switchBlock', '4000', (5, 1, 18, 16)),
            ('meterTrack', '551', None), ('meterLit', '550', None))
    for name, n, box in caps:
        rows, cols, ar, ac, err, tint = separable(n, box)
        print('%-12s %dx%d  separable rms %.1f levels  tint %s' % (name, len(cols), len(rows), err, np.round(tint, 3)))
        lines += ['inline constexpr float %sRows[] = { %s };' % (name, arr(rows)), 'inline constexpr float %sCols[] = { %s };' % (name, arr(cols, '%.3f')),
                  'inline constexpr float %sAlphaRows[] = { %s };' % (name, arr(ar, '%.2f')), 'inline constexpr float %sAlphaCols[] = { %s };' % (name, arr(ac, '%.2f')),
                  'inline constexpr Cap %s { %d, %d, %sRows, %sCols, %sAlphaRows, %sAlphaCols, { %s } };' % (name, len(cols), len(rows), name, name, name, name, arr(tint, '%.3f'))]
    panels_out = panels()
    # the caps as the reference draws them at 1x (fitted on its editor's frame at the defaults, where Range Max sits at
    # +12, Min at -12, the Rider and Output at 0): their outline and lip included; a colour per row (r, g, b) times a
    # factor per column; x0 the left column (absolute), dy the top row relative to the value's y on the scale
    frame = np.asarray(Image.open(os.path.join(ROOT, 'reference', 'wbassrider', 'shot_default_start.png')).convert('RGB')).astype(float)
    lines += ['// the caps fitted on the 1x frame of the reference editor: a colour per row (r, g, b) x a factor per column',
              'struct FrameCap { int x0, dy, w, h; const float* rows; const float* cols; };']
    for name, x0, x1, y0, y1, yref in (('riderFrame', 133, 187, 294, 363, 327), ('rangeMaxFrame', 41, 80, 200, 220, 222),
                                       ('rangeMinFrame', 41, 80, 431, 451, 432), ('outputFrame', 238, 277, 319, 339, 327)):
        reg = frame[y0:y1, x0:x1]
        w = x1 - x0; core = slice(w // 5, w - w // 5)
        rows = reg[:, core].mean(1)                                # (h, 3)
        cols = np.median(reg.mean(2) / np.maximum(rows.mean(1)[:, None], 1), 0)
        err = np.sqrt(np.mean((rows[:, None, :] * cols[None, :, None] - reg) ** 2))
        print('%-14s %dx%d from the frame, separable rms %.1f' % (name, w, y1 - y0, err))
        lines += ['inline constexpr float %sRows[] = { %s };' % (name, arr(rows.reshape(-1))), 'inline constexpr float %sCols[] = { %s };' % (name, arr(cols, '%.3f')),
                  'inline constexpr FrameCap %s { %d, %d, %d, %d, %sRows, %sCols };' % (name, x0, y0 - yref, w, y1 - y0, name, name)]
    # the knobs' dome as the reference draws it (fitted on its frame, all three knobs, each pointer masked at its own
    # angle): per radius (0.25 px steps, out to the black rim's outer edge) a grey level and four harmonics in angle
    # (0 = up, clockwise); inside radius 3 the plain mean (the pointer's root would leave a spot)
    import math
    gF = frame.mean(2)
    def bil(x, y):
        x0, y0 = int(math.floor(x)), int(math.floor(y)); fx, fy = x - x0, y - y0
        return (gF[y0, x0] * (1 - fx) * (1 - fy) + gF[y0, x0 + 1] * fx * (1 - fy) + gF[y0 + 1, x0] * (1 - fx) * fy + gF[y0 + 1, x0 + 1] * fx * fy)
    knobs = ((46.0, 111.5, 0.0), (200.0, 111.5, -132.0 + 10.0 / 47.0 * 264.0), (275.0, 111.5, 0.0))   # centre, pointer angle
    NH = 8
    def basis(th): return [1.0] + sum(([math.cos(k * th), math.sin(k * th)] for k in range(1, NH + 1)), [])
    radii = np.arange(0.0, 21.01, 0.25)
    coef = []
    for r in radii:
        rows = []; vals = []
        for (cx, cy, pa) in knobs:
            for ang in np.arange(0, 360, 3.0 if r > 3 else 20.0):
                th = math.radians(ang); rel = math.radians(ang - pa)
                if r < 19.0 and math.cos(rel) > -0.05 and abs(r * math.sin(rel)) < 2.8: continue      # the pointer
                rows.append(basis(th)); vals.append(bil(cx + r * math.sin(th) - 0.5, cy - r * math.cos(th) - 0.5))
        if r < 3.0:
            c = np.zeros(1 + 2 * NH); c[0] = float(np.mean(vals))
        else:
            c, *_ = np.linalg.lstsq(np.array(rows), np.array(vals), rcond=None)
        coef.append(c)
    coef = np.array(coef)
    print('knobDome: %d radii, centre grey %.0f, r 20: %.0f, r 21: %.0f' % (len(radii), coef[0, 0], coef[80, 0], coef[84, 0]))
    lines += ['// the dome of the knobs (fitted on the frame, the pointers masked): per radius from the centre in 0.25 px steps,',
              '// grey = c0 + sum over k = 1 .. 8 of (ck cos k a + sk sin k a), a from straight up, clockwise; centred 0.5 px above',
              '// the centre of the control rectangle',
              'inline constexpr float knobDomeStep = 0.25f;', 'inline constexpr int knobDomeRadii = %d, knobDomeHarmonics = %d;' % (len(radii), NH),
              'inline constexpr float knobDome[] = { %s };' % arr(coef.reshape(-1), '%.2f')]
    # the bezel round Spill and Artifacts (fitted on the frame's own pixels, the ticks' angles masked): the black rim and a
    # light metal ring outside it, as an offset over the panel with the knobs' cast shadow (the editor's knobShadowAt, so
    # it holds for each knob's own panel grey); per radius (0.5 px knots from 18.5 to 24, linear between) an offset and
    # eight harmonics in angle, one least-squares fit over every pixel of the ring
    pm = {n: dict(y0=y0, x0=x0, rows=rows_.mean(1), cols=cols_, h=len(rows_), w=len(cols_)) for n, y0, x0, rows_, cols_, _ in panels_out}['top']
    def panel_at(x, y):
        return pm['rows'][min(max(y - pm['y0'], 0), pm['h'] - 1)] * pm['cols'][min(max(x - pm['x0'], 0), pm['w'] - 1)]
    def shadow_at(x, y):   # WBassRiderEditor.cpp knobShadowAt
        f = 1.0
        for cx in (46.0, 200.0, 275.0):
            dx, dy = x - cx, y - 111.5; r = math.hypot(dx, dy)
            if r < 20.0 or abs(dx) > 24.0: continue
            rim = 0.32 * math.exp(-(r - 20.5) / 2.3) * min(max(dy / 20.0, 0.0), 1.0)
            sy = dy - min(max(dy, 0.0), 22.5); d = math.hypot(dx, sy)
            cast = 0.21 * math.exp(-max(0.0, dy - 22.0) / 22.0) * min(max((21.8 - d) / 1.6 + 0.5, 0.0), 1.0)
            f *= 1.0 - min(max(rim + cast, 0.0), 0.9)
        return f
    # the skin's background is off the frame row by row (a light band along the group frames' top line, brighter through
    # the knobs' labels, by up to 15 %): each row of the top panel is scaled by the frame over the model, the median over
    # the row between the knobs' columns (the Target track, the switch and the text left out); the rows the frame lines
    # sit on take their neighbours' scale
    top_rows = next(rows_ for n, y0, x0, rows_, cols_, _ in panels_out if n == 'top')
    boxes = [(70, 44, 256, 66), (100, 100, 150, 126)]
    ks = {}
    for yi in range(36, 162):
        rs = np.array([gF[yi, xi] / panel_at(xi, yi) for xi in range(8, 312)
                       if min(abs(xi + 0.5 - cx) for cx in (46.0, 200.0, 275.0)) >= 26.0 and not any(b_[0] <= xi < b_[2] and b_[1] <= yi < b_[3] for b_ in boxes)])
        rs = rs[rs < 1.15]; med = np.median(rs); k = np.median(rs[np.abs(rs - med) < 0.08])
        if 0.75 < k < 1.12: ks[yi] = k
    for yi in (74, 75, 76, 77, 78, 152, 153, 154, 155, 156): ks.pop(yi, None)
    known = sorted(ks)
    for yi in range(36, 162):
        top_rows[yi - 36] *= float(np.interp(yi, known, [ks[k_] for k_ in known]))
    # the skin has the frame lines baked into those rows: there the panel is its neighbours' (the editor draws the lines)
    for y0_, y1_ in ((73, 79), (152, 157)):
        for yi in range(y0_ + 1, y1_):
            t_ = (yi - y0_) / (y1_ - y0_)
            top_rows[yi - 36] = top_rows[y0_ - 36] * (1 - t_) + top_rows[y1_ - 36] * t_
    pm['rows'] = top_rows.mean(1)
    # what rows times columns can't say (the skin's panel darkens towards its ends and round the knobs, where the frame
    # does not): the frame over the model on a coarse grid, 8 px cells over the top panel (linear between their centres),
    # the knobs, their bezels, the text and the lines left out (a cell's median, then the pixels near it)
    FX = 8
    gxs = np.arange(6 + FX / 2, 314, FX); gys = np.arange(36 + FX / 2, 162, FX)
    fix = np.full((len(gys), len(gxs)), np.nan)
    for j, gy_ in enumerate(gys):
        for i, gx_ in enumerate(gxs):
            rs = []
            for yi in range(int(gy_ - FX / 2), int(gy_ + FX / 2)):
                for xi in range(max(int(gx_ - FX / 2), 8), min(int(gx_ + FX / 2), 312)):
                    if min(math.hypot(xi + 0.5 - c, yi + 0.5 - 111.5) for c in (46.0, 200.0, 275.0)) < 25.0: continue
                    rs.append(gF[yi, xi] / (panel_at(xi, yi) * shadow_at(xi + 0.5, yi + 0.5)))
            if len(rs) < 12: continue
            rs = np.array(rs); keep = rs[np.abs(rs - np.median(rs)) < 0.1]
            if len(keep) >= 12: fix[j, i] = np.median(keep)
    while np.isnan(fix).any():   # empty cells (under the knobs) take their neighbours' mean
        f2 = fix.copy()
        for j, i in zip(*np.where(np.isnan(fix))):
            nb = [fix[jj, ii] for jj, ii in ((j - 1, i), (j + 1, i), (j, i - 1), (j, i + 1)) if 0 <= jj < len(gys) and 0 <= ii < len(gxs) and not np.isnan(fix[jj, ii])]
            if nb: f2[j, i] = np.mean(nb)
        fix = f2
    fix = np.clip(fix, 0.7, 1.3)
    print('topPanelFix: %d x %d cells, %.2f .. %.2f' % (len(gxs), len(gys), fix.min(), fix.max()))
    def fix_at(x, y):
        fx = min(max((x - gxs[0]) / FX, 0.0), len(gxs) - 1.001); fy = min(max((y - gys[0]) / FX, 0.0), len(gys) - 1.001)
        i, j = int(fx), int(fy); tx, ty = fx - i, fy - j
        return (fix[j, i] * (1 - tx) + fix[j, i + 1] * tx) * (1 - ty) + (fix[j + 1, i] * (1 - tx) + fix[j + 1, i + 1] * tx) * ty
    panel_at_rc = panel_at
    def panel_at(x, y): return panel_at_rc(x, y) * fix_at(x + 0.5, y + 0.5)
    lines += ["// the top panel's correction: the frame over rows times columns on 8 px cells (centres from x 10, y 40), linear between",
              'inline constexpr float topPanelFixX0 = %.1ff, topPanelFixY0 = %.1ff, topPanelFixStep = %d.0f;' % (gxs[0], gys[0], FX),
              'inline constexpr int topPanelFixW = %d, topPanelFixH = %d;' % (len(gxs), len(gys)),
              'inline constexpr float topPanelFix[] = { %s };' % arr(fix.reshape(-1), '%.3f')]
    tick_angles = [0, 23, 46, 69, 91, 115, 135, -23, -46, -69, -91, -115, -135]
    dash_angles = [s_ * (11.35 + 22.7 * k_) for k_ in range(6) for s_ in (-1, 1)]   # (round the knob's centre)
    BSTEP = 0.5
    bradii = np.arange(18.5, 24.01, BSTEP)
    def fit_bezel(which, masked, NB):
        """per radius (0.5 px knots, linear between) an offset over the shadowed panel and NB harmonics in angle, one
        least-squares fit over every pixel of the ring round the given knobs, the masked angles left out"""
        def bbasis(th): return [1.0] + sum(([math.cos(k * th), math.sin(k * th)] for k in range(1, NB + 1)), [])
        nb = 1 + 2 * NB
        rows = []; vals = []
        for (cx, cy, pa) in which:
            for yi in range(int(cy) - 25, int(cy) + 26):
                for xi in range(int(cx) - 25, int(cx) + 26):
                    dx, dy = xi + 0.5 - cx, yi + 0.5 - cy
                    r = math.hypot(dx, dy)
                    if r < bradii[0] or r > bradii[-1]: continue
                    th = math.atan2(dx, -dy)
                    if masked(math.degrees(th), r): continue
                    fi = min((r - bradii[0]) / BSTEP, len(bradii) - 1 - 1e-6); i = int(fi); t = fi - i
                    row = np.zeros(len(bradii) * nb); bb = np.array(bbasis(th))
                    row[i * nb:(i + 1) * nb] = bb * (1 - t); row[(i + 1) * nb:(i + 2) * nb] = bb * t
                    rows.append(row); vals.append(gF[yi, xi] - panel_at(xi, yi) * shadow_at(xi + 0.5, yi + 0.5))
        A = np.array(rows); y = np.array(vals)
        lam = 2.0   # a little ridge on the harmonics, so sparse angles stay smooth
        R = np.diag([0.0 if (k % nb) == 0 else lam for k in range(A.shape[1])])
        c = np.linalg.solve(A.T @ A + R.T @ R, A.T @ y).reshape(len(bradii), nb)
        at = lambda deg, i: np.array(bbasis(math.radians(deg))) @ c[i]
        print('bezel: %d pixels, rms %.1f; r 21 offset at 0 deg %.0f, +-40 %.0f, +-100 %.0f, 180 %.0f'
              % (len(y), np.sqrt(np.mean((A @ c.reshape(-1) - y) ** 2)), at(0, 5), at(40, 5), at(100, 5), at(180, 5)))
        return c
    # Spill and Artifacts: the black rim and the light metal ring outside it (the ticks masked)
    bcoef = fit_bezel(knobs[1:], lambda deg, r: min(abs(deg - t) for t in tick_angles) < 5.0, 8)
    # Sensitivity: the black rim only, its ring being the dashes (masked from radius 19.4)
    scoef = fit_bezel(knobs[:1], lambda deg, r: r >= 19.4 and min(abs(deg - t) for t in dash_angles) < 9.5, 4)
    lines += ['// the bezel round Spill and Artifacts, the black rim and the light ring outside it, and round Sensitivity, the black',
              '// rim alone, as an offset over the shadowed panel: per radius from 18.5 in 0.5 px steps (linear between), an offset',
              '// and eight (Sensitivity: four) harmonics in angle (fitted on the frame, the ticks and dashes left out)',
              'inline constexpr float knobBezelR0 = 18.5f, knobBezelStep = %.2ff;' % BSTEP, 'inline constexpr int knobBezelRadii = %d, knobBezelHarmonics = 8, knobBezelSensHarmonics = 4;' % len(bradii),
              'inline constexpr float knobBezel[] = { %s };' % arr(bcoef.reshape(-1), '%.2f'),
              'inline constexpr float knobBezelSens[] = { %s };' % arr(scoef.reshape(-1), '%.2f')]
    # the Response switch (fitted on the frame at Slow): its block (20 x 15 at x 105, y 107) as a rank-2 model, the
    # slot's grey by distance from the block (15 columns), the frame's grey; Fast has the block 15 px right, the slot
    # mirrored
    blk = gF[107:122, 105:125]
    u, sv, vt = np.linalg.svd(blk, full_matrices=False)
    k = 2
    bu = u[:, :k] * sv[:k]; bv = vt[:k]
    err = np.sqrt(np.mean((bu @ bv - blk) ** 2))
    slot = gF[107:122, 125:140].mean(0)
    print('switch block rank-2 rms %.1f; slot %s' % (err, np.round(slot).astype(int)))
    lines += ['// the Response switch: its block (15 rows x 20 columns) as rows[k] x cols[k], k = 0, 1; the slot by distance from',
              '// the block; the frame grey',
              'inline constexpr float switchBlockU[] = { %s };' % arr(bu.reshape(-1), '%.3f'),
              'inline constexpr float switchBlockV[] = { %s };' % arr(bv.T.reshape(-1), '%.4f'),
              'inline constexpr float switchSlot[] = { %s };' % arr(slot),
              'inline constexpr float switchFrame = %.1ff;' % float(np.median(gF[122, 105:139]))]
    # the clip LED (measured on the frames, off and lit): a 1 px dark frame (x 280 .. 294, y 186 .. 195) round an inside
    # whose colour changes by row only (rows 187 .. 194, x 281 .. 293; its end columns 0.95 of it); lit, a red glow on
    # its sides (rows 190 .. 195) and below it (row 196)
    lit = np.asarray(Image.open(os.path.join(ROOT, 'reference', 'wbassrider', 'shot_clip_lit.png')).convert('RGB')).astype(float)
    off_rows = frame[187:195, 283:291].mean(1); on_rows = lit[187:195, 283:291].mean(1)
    glow_side = (lit[190:196, 280] + lit[190:196, 294]) / 2; glow_below = lit[196, 282:292].mean(0)
    lines += ['// the clip LED: rows 187 .. 194 inside (r, g, b), off and lit; the lit glow beside it (rows 190 .. 195) and below',
              'inline constexpr float ledOffRows[] = { %s };' % arr(off_rows.reshape(-1)), 'inline constexpr float ledOnRows[] = { %s };' % arr(on_rows.reshape(-1)),
              'inline constexpr float ledGlowSide[] = { %s };' % arr(glow_side.reshape(-1)), 'inline constexpr float ledGlowBelow[] = { %s };' % arr(glow_below),
              'inline constexpr float ledFrame = %.1ff;' % float(np.median(frame[186, 283:291].mean(1)))]
    print('led: off rows', np.round(off_rows.mean(1)).astype(int), 'on rows', np.round(on_rows[:, 0]).astype(int))
    # the caps' cast shadows (the reference's Shadow images, black with alpha): under the cap a profile per row, from
    # the cap image's last row on; along its sides a thin line of alpha (left, right)
    lines += ['// the cast shadows of the caps: alpha per row below the cap (from the first strong row), the side lines alpha']
    for name, shadow, capImg in (('rider', '2005', '2000'), ('range', '605', '600'), ('output', '3005', '3000')):
        _, al = load(shadow)
        _, cal = load(capImg)
        h = cal.shape[0]
        mid = slice(al.shape[1] // 3, 2 * al.shape[1] // 3)
        start = h - 6 + int(np.argmax(al[h - 6:, mid].mean(1) > 0.15))   # the first strong row: just under the cap
        prof = al[start:, mid].mean(1)
        prof = prof[: int(np.max(np.where(prof > 0.02)[0])) + 1]
        side = al[2:start, :].max(1)
        print('%-7s shadow from row %d (cap image %d rows): %d rows, peak %.2f, side %.2f' % (name, start, h, len(prof), prof.max(), np.median(side)))
        lines += ['inline constexpr float %sShadow[] = { %s };' % (name, arr(prof, '%.3f')),
                  'inline constexpr int %sShadowStart = %d;   // its row in the shadow image (the cap image has %d)' % (name, start, h),
                  'inline constexpr float %sShadowSide = %.3ff;' % (name, float(np.median(side)))]
    # the Target thumb (29 x 22): a 1 px frame (x 2 .. 26, y 1 .. 18) around a white glass whose opacity varies by row
    # (and rises towards the sides), a red centre line (x 14), a two-row shadow below
    rgb, al = load('552')
    g = rgb.mean(2)
    inner_rows = range(2, 18); inner_cols = [c for c in range(3, 26) if c != 14]
    grey = [float(np.mean([g[r, c] for c in inner_cols])) for r in inner_rows]
    alpha = [float(np.mean([al[r, c] for c in range(7, 12)])) for r in inner_rows]
    colk = [float(np.mean([al[r, c] / max(alpha[r - 2], 1e-3) for r in range(5, 15)])) for c in range(3, 26)]
    line = [float(g[r, 14]) for r in inner_rows]
    shadow = [float(al[19, 10]), float(al[20, 10])]
    frame = float(g[1, 10])
    print('targetThumb: glass alpha %.2f .. %.2f, frame grey %.0f' % (min(alpha), max(alpha), frame))
    lines += ['// the Target thumb: glass grey and alpha per row (y 2 .. 17), alpha factor per column (x 3 .. 25), the red',
              '// line brightness per row, the alpha of the two shadow rows, the frame grey',
              'inline constexpr float thumbGlassGrey[] = { %s };' % arr(grey), 'inline constexpr float thumbGlassAlpha[] = { %s };' % arr(alpha, '%.3f'),
              'inline constexpr float thumbGlassCol[] = { %s };' % arr(colk, '%.3f'), 'inline constexpr float thumbLine[] = { %s };' % arr(line),
              'inline constexpr float thumbShadow[] = { %s };' % arr(shadow, '%.3f'), 'inline constexpr float thumbFrame = %.1ff;' % frame]
    lines += ['// the panels: from y0 / x0 (1x), a colour per row (r, g, b) times a factor per column, and the grain (rms grey)',
              'struct Panel { int y0, x0, h, w; const float* rows; const float* cols; float grain; };']
    for name, y0, x0, rows, cols, grain in panels_out:
        lines += ['inline constexpr float %sPanelRows[] = { %s };' % (name, arr(rows.reshape(-1))),
                  'inline constexpr float %sPanelCols[] = { %s };' % (name, arr(cols, '%.3f')),
                  'inline constexpr Panel %sPanel { %d, %d, %d, %d, %sPanelRows, %sPanelCols, %.1ff };' % (name, y0, x0, len(rows), len(cols), name, name, grain)]
    lines += ['}', '']
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    print('wrote', OUT)


if __name__ == '__main__':
    main()
