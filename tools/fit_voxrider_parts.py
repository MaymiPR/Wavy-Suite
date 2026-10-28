"""Fits compact models of the reference Vocal Rider's editor (its skin, extracted into the git-ignored
reference/wvoxrider/skin/) and writes plugins/WVoxRider/WVoxRiderSkinData.h. No image is copied, only small measured
models that WVoxRiderEditor.cpp renders:

- the panels (from the 2x backgrounds at 1x, which match the reference's own frames to 0.2 grey levels): a colour per
  row times a factor per column times a coarse correction grid, and the grain left over, the printed art masked;
- the printed art as measured items: each label's string, ink box and grey; the lines as runs with their coverage;
  the knobs' scales as fitted strokes (a ring off the knob's centre, ticks, bent ends and flares); the two lamps;
- the small parts (fader caps and their shadows, the switch, the Target glass, the meter, the Automation buttons) as
  low-rank models: a few rows-times-columns products per premultiplied RGBA channel, any printed text masked (the
  editor prints it);
- the round parts (knobs, LEDs, lamps) as polar models round their centres (a value per radius and a few harmonics in
  angle per channel), the knobs' pointers as a difference layer along the pointer.

    python -I tools/fit_voxrider_parts.py
"""
import math
import os
import numpy as np
from PIL import Image
from scipy import ndimage as nd
from scipy.optimize import least_squares

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SKIN = os.path.join(ROOT, 'reference', 'wvoxrider', 'skin', 'Vocal Rider_Webp')
OUT = os.path.join(ROOT, 'plugins', 'WVoxRider', 'WVoxRiderSkinData.h')


def load(n):
    """a skin image at 1x as premultiplied RGBA 0 .. 1 (h, w, 4): a 2 x 2 box filter, as the reference draws them"""
    a = np.asarray(Image.open(os.path.join(SKIN, '%d.webp' % n)).convert('RGBA')).astype(float)
    h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
    a = a[:h, :w]
    al = a[..., 3:4] / 255.0
    pm = (a[..., :3] / 255.0 * al).reshape(h // 2, 2, w // 2, 2, 3).mean((1, 3))
    al = al.reshape(h // 2, 2, w // 2, 2, 1).mean((1, 3))
    return np.concatenate([pm, al], 2)


def opaque(rgb):
    """an opaque rgb (0 .. 255) array as premultiplied RGBA 0 .. 1"""
    return np.concatenate([rgb / 255.0, np.ones(rgb.shape[:2] + (1,))], 2)


def arr(v, fmt='%.1f'):
    return ', '.join((fmt % x) + 'f' for x in np.asarray(v, float).reshape(-1))


# ---------------------------------------------------------------------------------------------------------------------
def lowrank(P, k, mask=None):
    """P (h, w, 4) as sum over j < k of u[row, j] v[col, j] per channel, by alternating weighted least squares (mask: the
    pixels left out)"""
    h, w, _ = P.shape
    # masked pixels: filled along their row from the nearest unmasked ones (linear between) and weighted lightly, so
    # the model stays smooth under the text
    W = np.ones((h, w)) if mask is None else np.where(mask, 0.05, 1.0)
    P = P.copy()
    if mask is not None:
        for y in range(h):
            on = np.where(~mask[y])[0]
            if len(on) and mask[y].any():
                for c in range(4): P[y, mask[y], c] = np.interp(np.where(mask[y])[0], on, P[y, on, c])
    U = np.zeros((h, k, 4)); V = np.zeros((w, k, 4))
    lam = 1e-5
    for c in range(4):
        X = P[..., c]
        X0 = X
        u, s, vt = np.linalg.svd(X0, full_matrices=False)
        Uc = u[:, :k] * np.sqrt(s[:k]); Vc = vt[:k].T * np.sqrt(s[:k])
        for _ in range(40 if mask is not None else 0):
            for i in range(h):
                A = Vc * W[i][:, None]
                Uc[i] = np.linalg.solve(Vc.T @ A + lam * np.eye(k), A.T @ X[i])
            for j in range(w):
                A = Uc * W[:, j][:, None]
                Vc[j] = np.linalg.solve(Uc.T @ A + lam * np.eye(k), A.T @ X[:, j])
        U[:, :, c] = Uc; V[:, :, c] = Vc
    M = np.einsum('ikc,jkc->ijc', U, V)
    err = np.sqrt(np.mean(((M - P)[W > 0.5]) ** 2)) * 255
    return dict(w=w, h=h, k=k, u=U, v=V, err=err)


def emit_lowrank(lines, name, m):
    lines += ['inline constexpr float %sU[] = { %s };' % (name, arr(m['u'], '%.4f')),
              'inline constexpr float %sV[] = { %s };' % (name, arr(m['v'], '%.4f')),
              'inline constexpr LowRank %s { %d, %d, %d, %sU, %sV };' % (name, m['w'], m['h'], m['k'], name, name)]


# ---------------------------------------------------------------------------------------------------------------------
def bilinear(P, x, y):
    x -= 0.5; y -= 0.5
    h, w = P.shape[:2]
    x0, y0 = int(math.floor(x)), int(math.floor(y)); fx, fy = x - x0, y - y0
    def at(yy, xx): return P[min(max(yy, 0), h - 1), min(max(xx, 0), w - 1)]
    return at(y0, x0) * (1 - fx) * (1 - fy) + at(y0, x0 + 1) * fx * (1 - fy) + at(y0 + 1, x0) * (1 - fx) * fy + at(y0 + 1, x0 + 1) * fx * fy


def polar(P, cx, cy, rmax, NH=8, step=0.5, pointer=None):
    """P (h, w, 4) round (cx, cy) (pixel-edge coordinates) as, per radius knot (linear between), a value and NH harmonics
    in angle (0 = up, clockwise) per channel; with a pointer (dict half, r1, angle in degrees) its sector is left out and
    then sampled as the difference to the model, every 0.5 px along it from the centre and across it"""
    h, w, _ = P.shape
    radii = np.arange(0.0, rmax + step + 1e-6, step)
    nb = 1 + 2 * NH
    def basis(th): return np.array([1.0] + sum(([math.cos(k * th), math.sin(k * th)] for k in range(1, NH + 1)), []))
    pa = math.radians(pointer['angle']) if pointer else 0.0
    ux, uy = math.sin(pa), -math.cos(pa)
    rows, vals = [], []
    for yi in range(h):
        for xi in range(w):
            dx, dy = xi + 0.5 - cx, yi + 0.5 - cy
            r = math.hypot(dx, dy)
            if r > rmax: continue
            if pointer:
                t = dx * ux + dy * uy; nrm = abs(dx * uy - dy * ux)
                if t > 2.0 and nrm < pointer['half'] + 1.0 and t < pointer['r1'] + 1.5: continue   # (the pointer starts 4 px out)
            th = math.atan2(dx, -dy)
            fi = min(r / step, len(radii) - 1 - 1e-6); i = int(fi); tt = fi - i
            row = np.zeros(len(radii) * nb); bb = basis(th)
            row[i * nb:(i + 1) * nb] = bb * (1 - tt); row[(i + 1) * nb:(i + 2) * nb] = bb * tt
            rows.append(row); vals.append(P[yi, xi])
    A = np.array(rows); Y = np.array(vals)
    R = np.diag([1e-3 if (j % nb) == 0 else 0.02 for j in range(A.shape[1])])   # (a ridge: sparse knots stay tame)
    C = np.linalg.solve(A.T @ A + R.T @ R, A.T @ Y)
    err = np.sqrt(np.mean((A @ C - Y) ** 2)) * 255
    C = C.reshape(len(radii), nb, 4)
    m = dict(radii=len(radii), nh=NH, step=step, cx=cx, cy=cy, c=C, err=err, along=np.zeros(0), across=np.zeros(1), d=np.zeros((0, 1, 4)))
    if pointer:
        def model(x, y):
            dx, dy = x - cx, y - cy
            r = math.hypot(dx, dy); th = math.atan2(dx, -dy)
            fi = min(r / step, len(radii) - 1 - 1e-6); i = int(fi); tt = fi - i
            bb = basis(th)
            return bb @ C[i] * (1 - tt) + bb @ C[i + 1] * tt
        # on the frame's own pixel grid (1 px steps along and across, at pixel centres: no resampling blur in the
        # frame it was measured on), the pointer's strip and a pixel round it
        if abs(pointer['angle']) < 1e-6:
            xs = np.arange(math.floor(cx - pointer['half'] - 1.5), math.ceil(cx + pointer['half'] + 1.5)) + 0.5
            ys = np.arange(math.floor(cy - pointer['r1'] - 1.5), math.ceil(cy)) + 0.5
            along = cy - ys[::-1]; across = xs - cx
            D = np.array([[P[int(cy - a_), int(x_)] - model(x_, cy - a_) for x_ in xs] for a_ in along])
        else:                       # (a turned pointer: bilinear samples along and across it)
            along = np.arange(0.5, pointer['r1'] + 1.5 + 1e-6, 1.0)
            across = np.arange(-pointer['half'] - 1.0, pointer['half'] + 1.0 + 1e-6, 1.0)
            D = np.array([[bilinear(P, cx + r * ux - a * uy, cy + r * uy + a * ux) - model(cx + r * ux - a * uy, cy + r * uy + a * ux)
                           for a in across] for r in along])
        m.update(along=along, across=across, d=D)
    return m


def emit_polar(lines, name, m):
    lines += ['inline constexpr float %sC[] = { %s };' % (name, arr(m['c'], '%.4f'))]
    if len(m['along']):
        lines += ['inline constexpr float %sPointer[] = { %s };' % (name, arr(m['d'], '%.4f'))]
    lines += ['inline constexpr Polar %s { %d, %d, %.2ff, %.3ff, %.3ff, %sC, %d, %d, %.3ff, %.3ff, %s };'
              % (name, m['radii'], m['nh'], m['step'], m['cx'], m['cy'], name, len(m['along']), len(m['across']),
                 m['along'][0] if len(m['along']) else 0.0, m['across'][0], (name + 'Pointer') if len(m['along']) else 'nullptr')]


def circle(P, sel=None):
    """the centre and radius of a round part: a circle through its alpha's half-way contour"""
    al = P[..., 3]
    ys, xs = np.where((al > 0.3) & (al < 0.7))
    if sel is not None:
        k = sel(xs, ys); xs, ys = xs[k], ys[k]
    A = np.stack([xs + 0.5, ys + 0.5, np.ones(len(xs))], 1)
    c = np.linalg.lstsq(A, (xs + 0.5) ** 2 + (ys + 0.5) ** 2, rcond=None)[0]
    cx, cy = c[0] / 2, c[1] / 2
    return cx, cy, math.sqrt(c[2] + cx * cx + cy * cy)


# ---------------------------------------------------------------------------------------------------------------------
def base(n):
    a = np.asarray(Image.open(os.path.join(SKIN, '%d.webp' % n)).convert('RGB')).astype(float)
    return a.reshape(598, 2, 320, 2, 3).mean((1, 3))


def panel(rgb, name, y0, y1, x0, x1, FX, art=()):
    """a colour per row times a factor per column (medians: the printed art does not count), a correction over FX px
    cells (the median ratio, linear between cell centres), and the grain (rms) left over"""
    reg = rgb[y0:y1, x0:x1]
    g = reg.mean(2)
    # the art first stands out of its row (the tracks and boxes fill most of some columns, so a column's median would
    # learn them): a first mask from the rows alone
    m = np.abs(g / np.maximum(np.median(g, 1)[:, None], 1) - 1) < 0.15
    known = np.zeros(g.shape, bool)                                 # boxes of art given (the wells: too faint to stand out)
    for a, b, c, e in art: known[max(int(b) - y0, 0):max(int(math.ceil(e)) - y0, 0), max(int(a) - x0, 0):max(int(math.ceil(c)) - x0, 0)] = True
    m &= ~known
    for _ in range(4):
        rows = np.array([np.median(reg[i][m[i]], 0) if m[i].sum() > 4 else np.median(reg[i], 0) for i in range(len(reg))])
        lum = g / np.maximum(rows.mean(1)[:, None], 1)
        cols = np.array([np.median(lum[:, j][m[:, j]]) if m[:, j].sum() > 4 else 1.0 for j in range(g.shape[1])])
        model = rows.mean(1)[:, None] * cols[None, :]
        res = g - model
        sd = 1.4826 * np.median(np.abs(res))
        m = (np.abs(res) < max(4.0 * sd, 10.0)) & ~known
    gys = np.arange(FX / 2, y1 - y0, FX); gxs = np.arange(FX / 2, x1 - x0, FX)
    fix = np.ones((len(gys), len(gxs)))
    ratio = g / np.maximum(model, 1)
    for j, gy in enumerate(gys):
        for i, gx in enumerate(gxs):
            sl = (slice(int(gy - FX / 2), int(gy + FX / 2)), slice(int(gx - FX / 2), int(gx + FX / 2)))
            if m[sl].sum() > FX * FX / 4: fix[j, i] = np.median(ratio[sl][m[sl]])
    fix = np.clip(fix, 0.5, 1.5)
    yy, xx = np.mgrid[0:g.shape[0], 0:g.shape[1]]
    fx = np.clip((xx + 0.5 - gxs[0]) / FX, 0, len(gxs) - 1.001); fy = np.clip((yy + 0.5 - gys[0]) / FX, 0, len(gys) - 1.001)
    i, j = fx.astype(int), fy.astype(int); tx, ty = fx - i, fy - j
    F = (fix[j, i] * (1 - tx) + fix[j, i + 1] * tx) * (1 - ty) + (fix[j + 1, i] * (1 - tx) + fix[j + 1, i + 1] * tx) * ty
    res = (reg - rows[:, None, :] * cols[None, :, None] * F[..., None]).mean(2)
    grain = float(np.sqrt(np.mean(np.clip(res[m], -40, 40) ** 2)))
    print('%-12s %d x %d, art %.1f %%, grain rms %.1f' % (name, len(cols), len(rows), 100 * (~m).mean(), grain))
    return dict(name=name, y0=y0, x0=x0, rows=rows, cols=cols, fix=fix, fx0=gxs[0] + x0, fy0=gys[0] + y0, step=FX, grain=grain,
                model=rows[:, None, :] * cols[None, :, None] * F[..., None])


def emit_panel(lines, p):
    n = p['name']
    lines += ['inline constexpr float %sRows[] = { %s };' % (n, arr(p['rows'])),
              'inline constexpr float %sCols[] = { %s };' % (n, arr(p['cols'], '%.4f')),
              'inline constexpr float %sFix[] = { %s };' % (n, arr(p['fix'], '%.3f')),
              'inline constexpr Panel %s { %d, %d, %d, %d, %sRows, %sCols, %d, %d, %.1ff, %.1ff, %d.0f, %sFix, %.1ff };'
              % (n, p['y0'], p['x0'], len(p['rows']), len(p['cols']), n, n, p['fix'].shape[1], p['fix'].shape[0], p['fx0'], p['fy0'], p['step'], n, p['grain'])]


def components(gray, region, bright, thr=25, knobs=(), skip=()):
    """the printed ink in a region: connected parts (letters merged across 1 px) standing out of the background (a
    grey opening / closing) by thr; their ink boxes, greys (the 85th / 15th percentile) and pixel coverage"""
    x0, y0, x1, y1 = region
    g = gray[y0:y1, x0:x1]
    bg = nd.grey_opening(g, size=(7, 7)) if bright else nd.grey_closing(g, size=(7, 7))
    d = (g - bg) if bright else (bg - g)
    m = d > thr
    yy, xx = np.mgrid[y0:y1, x0:x1]
    for cx, cy, r in knobs: m &= np.hypot(xx + 0.5 - cx, yy + 0.5 - cy) > r
    for a, b, c, e in skip: m &= ~((xx >= a) & (xx < c) & (yy >= b) & (yy < e))
    lab, n = nd.label(nd.binary_dilation(m, np.ones((1, 3))), np.ones((3, 3)))
    out = []
    for i in range(1, n + 1):
        s = (lab == i) & m
        if s.sum() < 3: continue
        ys, xs = np.where(s)
        v = g[s]
        grey = float(np.percentile(v, 85) if bright else np.percentile(v, 15))
        out.append(dict(x0=x0 + xs.min(), y0=y0 + ys.min(), x1=x0 + xs.max() + 1, y1=y0 + ys.max() + 1, grey=grey, n=int(s.sum()),
                        cx=x0 + (xs.min() + xs.max() + 1) / 2, cy=y0 + (ys.min() + ys.max() + 1) / 2,
                        cov=np.clip(d / max(abs(grey - np.median(bg[s])), 1), 0, 1) * s, ox=x0, oy=y0))
    return out


def labels(gray, items, comps):
    """the labels: for each (text, centre, bold) the detected part nearest the centre"""
    out = []
    for text, cx, cy, bold in items:
        best = min(comps, key=lambda c: math.hypot(c['cx'] - cx, c['cy'] - cy))
        if math.hypot(best['cx'] - cx, best['cy'] - cy) > 8: raise SystemExit('no ink for %r near %s' % (text, (cx, cy)))
        out.append((text, best['x0'], best['y0'], best['x1'], best['y1'], best['grey'], bold))
    return out


def runs(comp):
    """a printed line as runs of pixels: per row (a horizontal line) or per column (a vertical one) its start, length and
    the coverage of its pixels (median), the end pixels' coverage extending it fractionally"""
    cov = comp['cov']
    ys, xs = np.where(cov > 0)
    w, h = xs.max() - xs.min() + 1, ys.max() - ys.min() + 1
    out = []
    if w >= h:
        for y in range(ys.min(), ys.max() + 1):
            row = cov[y]; on = np.where(row > 0.05)[0]
            if len(on) == 0: continue
            a, b = on.min(), on.max()
            core = row[a:b + 1]
            alpha = float(np.median(core[core > 0.5])) if (core > 0.5).any() else float(core.mean())
            x0 = a + 1 - row[a] / max(alpha, 1e-3); x1 = b + row[b] / max(alpha, 1e-3)
            out.append((comp['ox'] + x0, comp['oy'] + y, x1 - x0, 1.0, alpha, comp['grey']))
    else:
        for x in range(xs.min(), xs.max() + 1):
            col = cov[:, x]; on = np.where(col > 0.05)[0]
            if len(on) == 0: continue
            a, b = on.min(), on.max()
            core = col[a:b + 1]
            alpha = float(np.median(core[core > 0.5])) if (core > 0.5).any() else float(core.mean())
            y0 = a + 1 - col[a] / max(alpha, 1e-3); y1 = b + col[b] / max(alpha, 1e-3)
            out.append((comp['ox'] + x, comp['oy'] + y0, 1.0, y1 - y0, alpha, comp['grey']))
    return out


# ---------------------------------------------------------------------------------------------------------------------
def scale_strokes(p, nt, X, Y):
    """a knob's printed scale (knob-centre coordinates): a ring arc round (0, d) radius R over +-aEnd (ring-centre
    angles), ticks along rays from (0, dT) at angles A[|k|] from the ring out by L[|k|]; at each end the ring bent in
    (towards aEnd + phi2, Li long) and a flare out from there (towards aEnd + phi, Le long, its own width and grey).
    Returns the coverage of the ring and ticks (grey G) and of the flares (grey GE)"""
    d, R, wR, G, aEnd, wT, Le, wE, GE, phi, dT = p[:11]
    A = np.concatenate([[0.0], p[11:11 + nt]]); L = p[11 + nt:12 + 2 * nt]; phi2, Li = p[12 + 2 * nt:14 + 2 * nt]
    dx, dy = X, Y - d
    r = np.hypot(dx, dy); th = np.degrees(np.arctan2(dx, -dy))
    ea = math.radians(aEnd)
    e1 = np.hypot(dx - R * math.sin(ea), dy + R * math.cos(ea)); e2 = np.hypot(dx + R * math.sin(ea), dy + R * math.cos(ea))
    dist = np.where(np.abs(th) <= aEnd, np.abs(r - R), np.minimum(e1, e2))
    cG = np.clip((wR / 2 - dist) * 4 + 0.5, 0, 1)
    def seg(px, py, a_deg, l, w):
        a = math.radians(a_deg); ux, uy = math.sin(a), -math.cos(a)
        qx, qy = X - px, Y - py
        t = qx * ux + qy * uy; n = np.abs(qx * uy - qy * ux)
        return np.clip((w / 2 - np.hypot(t - np.clip(t, 0, l), n)) * 4 + 0.5, 0, 1)
    def ring_hit(a_deg):
        a = math.radians(a_deg); ux, uy = math.sin(a), -math.cos(a)
        oy = dT - d; bq = oy * uy; cq = oy * oy - R * R
        s = -bq + math.sqrt(max(bq * bq - cq, 0.0))
        return s * ux, dT + s * uy
    for k in range(-nt, nt + 1):
        a = math.copysign(A[abs(k)], k) if k else 0.0
        hx, hy = ring_hit(a)
        cG = np.maximum(cG, seg(hx, hy, a, L[abs(k)], wT))
    cE = np.zeros_like(X)
    for s_ in (-1, 1):
        ex, ey = s_ * R * math.sin(ea), d - R * math.cos(ea)
        b2 = math.radians(s_ * (aEnd + phi2))
        cG = np.maximum(cG, seg(ex, ey, s_ * (aEnd + phi2), Li, wR))
        cE = np.maximum(cE, seg(ex + Li * math.sin(b2), ey - Li * math.cos(b2), s_ * (aEnd + phi), Le, wE))
    return cG, G, cE, GE


def fit_scale(gray, cx, cy, nt, p0, rin, rout, exclude=()):
    bg = nd.grey_opening(gray, size=(5, 5))
    pix = [(x, y) for y in range(int(cy - rout - 1), int(cy + rout + 2)) for x in range(int(cx - rout - 1), int(cx + rout + 2))
           if rin <= math.hypot(x + 0.5 - cx, y + 0.5 - cy) <= rout and not any(a <= x < b and c <= y < e for a, c, b, e in exclude)]
    pix = np.array(pix)
    ss = (np.arange(4) + 0.5) / 4; sx, sy = np.meshgrid(ss, ss)
    X = (pix[:, 0:1] + sx.reshape(1, -1)) - cx; Y = (pix[:, 1:2] + sy.reshape(1, -1)) - cy
    obs = gray[pix[:, 1], pix[:, 0]]; b = bg[pix[:, 1], pix[:, 0]]
    def model(p):
        cG, G, cE, GE = scale_strokes(p, nt, X, Y)
        v = b[:, None] * (1 - cG) + G * cG
        return (v * (1 - cE) + GE * cE).mean(1)
    lo = [0, 10, 0.5, 60, 100, 0.5, 0, 0.5, 30, -80, -6] + [0] * nt + [0] * (nt + 1) + [-180, 0]
    hi = [6, 32, 4, 255, 150, 4, 15, 4, 255, 80, 6] + [150] * nt + [12] * (nt + 1) + [180, 10]
    res = least_squares(lambda p: model(p) - obs, p0, bounds=(lo, hi), diff_step=1e-3)
    return res.x, float(np.sqrt(np.mean(res.fun ** 2))), float(np.sqrt(np.mean((b - obs) ** 2)))


# ---------------------------------------------------------------------------------------------------------------------
STUDIO = dict(vocal=(110.0, 116.87), music=(210.0, 116.87))
# the fader wells (measured on the background: 21.5 px wide, y 210 .. 463)
WELLS = ((51.0, 210.0, 72.5, 463.0), (149.5, 210.0, 171.0, 463.0), (247.0, 210.0, 268.5, 463.0))
LIVE = dict(vocal=(160.0, 116.87), music=(257.0, 116.87), spill=(88.09, 127.82))


def main():
    lines = ['#pragma once',
             "// Generated by tools/fit_voxrider_parts.py: models fitted on the reference Vocal Rider's skin (no image data; the",
             "// editor's 1x scale; premultiplied channels 0 .. 1 unless said otherwise). Do not edit.",
             'namespace wvoxrider::skin', '{',
             '// a low-rank part: channel c (r, g, b, a premultiplied) of pixel (x, y) = sum over j < k of u[(y k + j) 4 + c] v[(x k + j) 4 + c]',
             'struct LowRank { int w, h, k; const float* u; const float* v; };',
             '// a round part: per radius knot (step apart, linear between) a value and nh harmonics in angle (a from straight',
             '// up, clockwise: c0 + sum ck cos k a + sk sin k a) per channel, round (cx, cy) in its own 1x image; a pointer (if',
             '// any) as the difference to it, sampled every 1 px along it (straight up) from along0 and across it (to its',
             '// right) from across0',
             'struct Polar { int radii, nh; float step, cx, cy; const float* c; int along, across; float along0, across0; const float* pointer; };',
             '// a panel: a colour per row (r, g, b, 0 .. 255) times a factor per column, times a correction over cells (centres',
             '// from fx0, fy0, a cell apart, linear between), and the grain (rms grey) left over',
             'struct Panel { int y0, x0, h, w; const float* rows; const float* cols; int fixW, fixH; float fx0, fy0, step; const float* fix; float grain; };',
             '// a printed label: its text, ink box, grey (0 .. 255), bold or not; a printed line run: x, y, w, h, grey, coverage',
             'struct Label { const char* text; float x0, y0, x1, y1, grey; bool bold; };',
             'struct Run { float x, y, w, h, grey, alpha; };',
             '// a knob scale (knob-centre coordinates, degrees from straight up, clockwise): a ring arc round (0, dy) radius r,',
             '// width w and grey over +-end; ticks along rays from (0, tickDy) at angles[i] (i 0 .. n, both sides) from the ring',
             '// out by lengths[i], tickW wide; at each end the ring bent in towards end + bendAngle for bendLength, then a flare',
             '// towards end + flareAngle, flareLength long, flareW wide and flareGrey',
             'struct Scale { float dy, r, w, grey, end, tickW, flareLength, flareW, flareGrey, flareAngle, tickDy; int n; float angles[8], lengths[8]; float bendAngle, bendLength; };']

    studio, live = base(15000), base(15002)
    gS, gL = studio.mean(2), live.mean(2)

    # ---- panels
    for p in (panel(studio, 'topPanel', 36, 171, 6, 314, 4), panel(live, 'topPanelLive', 36, 171, 6, 314, 4),
              panel(studio, 'silverPanel', 171, 551, 4, 316, 8, art=WELLS), panel(studio, 'leatherPanel', 551, 598, 0, 320, 8)):
        emit_panel(lines, p)
        if p['name'] == 'silverPanel':
            # the wells' shade: the background over the panel's model inside them, the tracks and marks left out
            ratios = []
            for a, b_, c, e in WELLS:
                sub = studio[int(b_) + 6:int(e) - 6, int(a) + 1:int(c) - 1].mean(2) / p['model'][int(b_) + 6 - 171:int(e) - 6 - 171, int(a) + 1 - 4:int(c) - 1 - 4].mean(2)
                ratios.append(sub[(sub > 0.75) & (sub < 1.05)])
            shade = float(np.median(np.concatenate(ratios)))
            print('wells: shade %.3f' % shade)
            lines.append('inline constexpr float wellShade = %.3ff;   // the panel times this inside the wells' % shade)

    # ---- labels and lines
    def top_items(knobs, n0):
        cs = components(gS if n0 == 's' else gL, (6, 38, 314, 170), True, knobs=[(x, y, 38.0 if x != LIVE['spill'][0] else 25.0) for x, y in knobs])
        zs = components(gS if n0 == 's' else gL, (6, 70, 314, 84), True)            # the zeros, above the ticks
        return cs, zs
    sc, sz = top_items(STUDIO.values(), 's')
    lc, lz = top_items(LIVE.values(), 'l')
    silver = components(gS, (4, 176, 316, 548), False, skip=((40, 210, 80, 460), (140, 210, 182, 460), (238, 210, 278, 460)))
    S_items = [('Target', 35, 53, True), ('Ride', 49, 78.5, True), ('Idle', 46.5, 94.5, True), ('Fast', 48.5, 130.5, True),
               ('Slow', 49.5, 144.5, True), ('Sensitivity', 161, 82, True), ('-', 80, 145, True), ('+', 142, 144, True),
               ('-', 180, 145, True), ('+', 242, 144, True), ('Vocal', 109, 157, True), ('Music', 211, 157, True)]
    L_items = [('Target', 35, 53, True), ('Ride', 43.5, 78.5, True), ('Idle', 41.5, 94.5, True), ('Fast', 43.5, 141.5, True),
               ('Slow', 44.5, 157.5, True), ('Sensitivity', 209, 81, True), ('-', 67.5, 145, True), ('+', 110, 145, True),
               ('-', 130, 145, True), ('+', 192, 144, True), ('-', 227, 145, True), ('+', 289, 144, True),
               ('Spill', 87.5, 158, True), ('Vocal', 160, 157, True), ('Music', 257.5, 157, True)]
    V_items = [('Range', 63, 200, True), ('Rider', 161, 199, True), ('Output', 260, 200, True), ('Automation', 159.5, 515, True)]
    for db, y in ((12, 231), (6, 283.5), (0, 336), (-6, 388.5), (-12, 441)):
        t = ('+%d' % db) if db > 0 else str(db)
        V_items += [(t, 118.5 if db != 12 else 118.5, y, False), (t, 202.5, y, False)]
        if abs(db) in (0, 12): V_items.append((t, 293.5 if db else 287.5, y, False))
    # the silver's scale numbers: the "+12" next to the left mark merges with it; take the text's own box
    silver_text = [c for c in silver if c['y1'] - c['y0'] >= 6]
    def fixed_plus12(items):
        """the left "+12" merges with its mark (its box ends at the mark, x 128); each "-12"'s minus stands 2 px off
        its digits (4 px left of their box)"""
        out = []
        for t, x0, y0, x1, y1, grey, bold in items:
            if t == '+12' and x0 < 128 < x1: x1, grey = 127, 66.0
            if t == '-12': x0 -= 4
            out.append((t, x0, y0, x1, y1, grey, bold))
        return out
    labS = labels(gS, S_items, sc) + labels(gS, [('0', 110, 80.5, True), ('0', 210, 80.5, True)], sz)
    labL = labels(gL, L_items, lc) + labels(gL, [('0', 160, 80.5, True), ('0', 257, 80.5, True)], lz)
    labV = fixed_plus12(labels(gS, V_items, silver_text))
    for name, lab in (('labels', labS), ('labelsLive', labL), ('labelsSilver', labV)):
        lines += ['inline constexpr Label %s[] = {' % name]
        lines += ['    { "%s", %.1ff, %.1ff, %.1ff, %.1ff, %.1ff, %s },' % (t, a, b, c, d, gr, 'true' if bo else 'false') for t, a, b, c, d, gr, bo in lab]
        lines += ['};']
    print('labels: %d / %d / %d' % (len(labS), len(labL), len(labV)))
    # lines: the Sensitivity brackets (top), the line over the Target meter (dark), the Automation brackets
    def line_runs(comps, test):
        rs = []
        for c in comps:
            if test(c): rs += runs(c)
        return rs
    # a line: thin (its pixels about its box's width plus height) and 6 px long at least
    thin = lambda c: c['n'] <= 1.6 * ((c['x1'] - c['x0']) + (c['y1'] - c['y0'])) and max(c['x1'] - c['x0'], c['y1'] - c['y0']) >= 6
    brS = line_runs(components(gS, (6, 78, 314, 90), True, knobs=[(x, y, 30.5) for x, y in STUDIO.values()]), thin)
    brL = line_runs(components(gL, (6, 78, 314, 90), True, knobs=[(x, y, 30.5) for x, y in LIVE.values()]), thin)
    tgt = line_runs(components(gS, (40, 40, 290, 46), False, thr=20), lambda c: thin(c) and c['x1'] - c['x0'] > 50)
    aut = line_runs(components(gS, (90, 512, 232, 522), False), thin)
    for name, rs in (('linesTop', brS + tgt), ('linesTopLive', brL + tgt), ('linesSilver', aut)):
        lines += ['inline constexpr Run %s[] = { %s };' % (name, ', '.join('{ %.2ff, %.2ff, %.2ff, %.2ff, %.1ff, %.3ff }' % (x, y, w, h, gr, a)
                                                                         for (x, y, w, h, a, gr) in rs))]
    print('line runs: %d / %d / %d' % (len(brS + tgt), len(brL + tgt), len(aut)))

    # ---- the knobs' printed scales
    p0 = [3.2, 27.0, 1.94, 141.2, 125.8, 2.08, 6.87, 2.17, 95.0, 14.7, 3.9, 22.84, 45.27, 67.72, 90.4, 113.69, 9.1, 3.45, 5.82, 3.43, 6.67, 3.32, 91.25, 4.56]
    big, rms, rms0 = fit_scale(gS, *STUDIO['vocal'], 5, p0, 21.5, 38.0, exclude=[(104, 74, 116, 84), (74, 138, 86, 150), (134, 138, 148, 150)])
    big[16] = min(big[16], (STUDIO['vocal'][1] + big[0] - big[1]) - 84.6)   # the top tick ends under the "0" (y 77 .. 84)
    print('big knob scale rms %.1f (background alone %.1f): %s' % (rms, rms0, np.round(big, 2).tolist()))
    p0s = [2.0, 18.0, 1.9, 141.0, 125.0, 2.0, 5.0, 2.0, 95.0, 14.0, 2.0, 45.0, 90.0, 0.0, 0.0, 6.0, 4.0, 4.0, 0.0, 0.0, 90.0, 3.0]
    spl, rms, rms0 = fit_scale(gL, *LIVE['spill'], 4, p0s, 15.0, 30.0, exclude=[(62, 140, 74, 150), (104, 138, 116, 150)])
    print('spill scale rms %.1f (background alone %.1f): %s' % (rms, rms0, np.round(spl, 2).tolist()))
    def emit_scale(name, p, nt):
        A = [0.0] + list(p[11:11 + nt]); L = list(p[11 + nt:12 + 2 * nt])
        A += [0.0] * (8 - len(A)); L += [0.0] * (8 - len(L))
        lines.append('inline constexpr Scale %s { %s, %d, { %s }, { %s }, %.2ff, %.2ff };' % (name, arr(p[:11], '%.3f'), nt, arr(A, '%.2f'), arr(L, '%.2f'), p[12 + 2 * nt], p[13 + 2 * nt]))
    emit_scale('bigScale', big, 5)
    emit_scale('spillScale', spl, 4)

    # ---- the knobs: Vocal / Music (54 x 60 at the control's top left), the disabled Music knob (opaque, the scale and
    # panel included), Spill (36 x 36); pointers straight up in the middle frames
    P = load(10136); cx, cy, r = circle(P, lambda xs, ys: ys < 26)
    kb = polar(P, cx, cy, 26.5, pointer=dict(half=2.0, r1=21.0, angle=0.0))
    P0, P1 = load(10101), load(10171)
    def pointer_angle(P):
        best = None
        for a in np.arange(-150, 150.1, 0.5):
            u, v = math.sin(math.radians(a)), -math.cos(math.radians(a))
            val = np.mean([bilinear(P, cx + rr * u, cy + rr * v)[:3].mean() for rr in np.arange(8, 19, 1.0)])
            if best is None or val > best[1]: best = (a, val)
        return best[0]
    a0, a1 = pointer_angle(P0), pointer_angle(P1)
    print('big knob: centre %.2f %.2f r %.2f, polar rms %.1f; pointer from %.1f to %.1f degrees' % (cx, cy, r, kb['err'], a0, a1))
    emit_polar(lines, 'bigKnob', kb)
    half = (a1 - a0) / 2            # the ends measured to the pixel; the middle frame points straight up
    lines.append('inline constexpr float knobAngle0 = %.2ff, knobAngle1 = %.2ff;   // the pointer at the ends (degrees)' % (-half, half))
    D = load(10100)
    def dark_angle(P):
        best = None
        for a in np.arange(-150, 150.1, 0.5):
            u, v = math.sin(math.radians(a)), -math.cos(math.radians(a))
            val = np.mean([bilinear(P, cx + rr * u, cy + rr * v)[:3].mean() for rr in np.arange(8, 19, 1.0)])
            if best is None or val > best[1]: best = (a, val)
        return best[0]
    da = dark_angle(D)
    kd = polar(D, cx, cy, 27.5, pointer=dict(half=2.0, r1=21.0, angle=da))
    print('disabled knob: pointer at %.1f degrees, polar rms %.1f' % (da, kd['err']))
    emit_polar(lines, 'bigKnobDisabled', kd)
    lines.append('inline constexpr float knobDisabledAngle = %.1ff;' % da)
    Q = load(11035); qx, qy, qr = circle(Q, lambda xs, ys: ys < 15)
    ks = polar(Q, qx, qy, 17.5, NH=6, pointer=dict(half=1.5, r1=14.0, angle=0.0))
    print('spill knob: centre %.2f %.2f r %.2f, polar rms %.1f' % (qx, qy, qr, ks['err']))
    emit_polar(lines, 'spillKnob', ks)

    # ---- LEDs and lamps (polar): Ride (green) on / off, Idle (yellow) on / off, the clip LED, the Music LED off / on,
    # the two lamps on the silver panel (from the background)
    R0, R1 = load(450), load(451)
    rnd = [('ledRideOn', R0[:18], 6), ('ledRideOff', R1[:18], 6), ('ledIdleOff', R0[18:], 6), ('ledIdleOn', R1[18:], 6),
           ('clipOff', load(200), 6), ('clipOn', load(201), 6), ('musicLedOff', load(10000), 8), ('musicLedOn', load(10023), 8)]
    for name, P, nh in rnd:
        h, w = P.shape[:2]
        cx_, cy_, r_ = circle(P)
        rmax = min(max(h, w) * 0.75, math.hypot(w, h) / 2)
        m = polar(P, cx_, cy_, rmax, NH=nh)
        print('%-12s %d x %d centre %.2f %.2f r %.2f, polar rms %.1f' % (name, w, h, cx_, cy_, r_, m['err']))
        emit_polar(lines, name, m)
        lines.append('inline constexpr int %sW = %d, %sH = %d;' % (name, w, name, h))
    for name, (x0, y0) in (('lampTop', (154, 177)), ('lampBottom', (154, 496))):
        P = opaque(studio[y0:y0 + 13, x0:x0 + 13])
        m = polar(P, 6.5, 6.5, 6.5, NH=6)
        print('%-12s polar rms %.1f' % (name, m['err']))
        emit_polar(lines, name, m)
        lines.append('inline constexpr float %sX = %d.0f, %sY = %d.0f;' % (name, x0, name, y0))

    # ---- the small parts (low rank)
    parts = [('riderCap', 2000, 3), ('outputCap', 3000, 3), ('rangeMaxCap', 600, 3), ('rangeMinCap', 606, 3),
             ('riderShadow', 2005, 2), ('outputShadow', 3005, 2), ('rangeMaxShadow', 605, 2), ('rangeMinShadow', 610, 2),
             ('switchSlow', 351, 3), ('switchFast', 350, 3), ('targetGlass', 552, 3), ('meterLit', 550, 3), ('meterTrack', 551, 3)]
    for name, n, k in parts:
        m = lowrank(load(n), k)
        print('%-18s %3d x %-3d rank %d rms %.1f' % (name, m['w'], m['h'], k, m['err']))
        emit_lowrank(lines, name, m)
    # the Automation buttons, all off (420) and all lit (421), their text masked; the text's box and colour per button
    for name, n in (('automationOff', 420), ('automationOn', 421)):
        P = load(n)
        g = P[..., :3].mean(2) / np.maximum(P[..., 3], 1e-3) * 255
        mask = np.zeros(g.shape, bool)
        texts = []
        # the text boxes (read off the pixels: 6 px capitals on the face, 2 px lower on a lit, pressed button) and
        # the ink's colour (the darkest / lightest fifth of the box: OFF and READ dark, WRITE light)
        dy = 2 if n == 421 else 0
        for (x0, y0, x1, y1), t, dark in (((16, 6, 32, 12), 'OFF', True), ((52, 6, 79, 12), 'WRITE', False), ((96, 6, 119, 12), 'READ', True)):
            y0 += dy; y1 += dy
            box = np.zeros(g.shape, bool); box[y0:y1, x0:x1] = True
            v = g[box]
            rgbv = (P[..., :3] / np.maximum(P[..., 3:4], 1e-3) * 255)[box]
            sel = v <= np.percentile(v, 20) if dark else v >= np.percentile(v, 80)
            texts.append((t, x0, y0, x1, y1, rgbv[sel].mean(0)))
            mask |= box
        mask = nd.binary_dilation(mask, np.ones((3, 3)))
        m = lowrank(P, 4, mask)
        print('%-18s %3d x %-3d rank 4 rms %.1f (text %d px masked)' % (name, m['w'], m['h'], m['err'], mask.sum()))
        emit_lowrank(lines, name, m)
        lines.append('inline constexpr Label %sText[] = { %s };' % (name, ', '.join('{ "%s", %.1ff, %.1ff, %.1ff, %.1ff, %.1ff, true }' % (t, a, b, c, d, col.mean()) for t, a, b, c, d, col in texts)))
        lines.append('inline constexpr float %sTextRgb[] = { %s };' % (name, arr(np.array([col for *_, col in texts]))))
    # the Idle arrow (12 x 11): a triangle, its colour by row
    P = load(700)
    rgb = P[..., :3] / np.maximum(P[..., 3:4], 1e-3) * 255
    rows = np.array([rgb[y][P[y, :, 3] > 0.9].mean(0) if (P[y, :, 3] > 0.9).any() else [0, 0, 0] for y in range(P.shape[0])])
    lines.append('inline constexpr float idleArrowRows[] = { %s };   // (r, g, b) per row' % arr(rows))
    lines += ['}', '']
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    print('wrote', OUT, '(%d kB)' % (os.path.getsize(OUT) // 1024))


if __name__ == '__main__':
    main()
