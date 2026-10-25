"""Fits the segment font of the reference H-Delay's time display (reference/hdelay, git-ignored screenshots) and prints
the geometry WDelay's display draws with. No pixels are kept: only cell sizes, stroke widths, slants, segment insets
and positions.

A character is a 7-segment cell (w x h, its top left at x, y before the slant), strokes t wide; horizontal strokes run
from hx to w - hx, vertical ones from vy to h/2 - vy and from h/2 + vy to h - vy; the cell leans right by `slant` px per
px of height. The amber coverage of the reference (R - B of the pixels: the glass is grey) is matched by a supersampled
rendering of the same polygons.

    python -I tools/fit_hdelay_display.py [big note bar small pp dual | ppbox]
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
from scipy import optimize

SS = 8                                       # supersampling of the rendering
SEG = {'0': 'ABCDEF', '1': 'BC', '2': 'ABGED', '3': 'ABGCD', '4': 'FGBC', '5': 'AFGCD', '6': 'AFGEDC', '7': 'ABC',
       '8': 'ABCDEFG', '9': 'ABCDFG', 'D': 'ABCDEF', 'B': 'ABCDEFG', 'A': 'ABCEFG', 'R': 'ABEFGK', 'T': 'AIJ', 'L': 'DEF',
       'P': 'ABEFG', 'M': 'ABCEFI', '-': 'G', ' ': ''}


def segments(ch, w, h, t, hx, vy):
    """the strokes of a character as rectangles (x0, y0, x1, y1) in the cell, and the R's leg as a quad"""
    rects, quads = [], []
    for s in SEG.get(ch, ''):
        if s == 'A': rects.append((hx, 0, w - hx, t))
        elif s == 'G': rects.append((hx, h / 2 - t / 2, w - hx, h / 2 + t / 2))
        elif s == 'D': rects.append((hx, h - t, w - hx, h))
        elif s == 'F': rects.append((0, vy, t, h / 2 - vy))
        elif s == 'E': rects.append((0, h / 2 + vy, t, h - vy))
        elif s == 'B': rects.append((w - t, vy, w, h / 2 - vy))
        elif s == 'C': rects.append((w - t, h / 2 + vy, w, h - vy))
        elif s == 'I': rects.append((w / 2 - t / 2, vy, w / 2 + t / 2, h / 2 - vy))
        elif s == 'J': rects.append((w / 2 - t / 2, h / 2 + vy, w / 2 + t / 2, h - vy))
        elif s == 'K':
            x0, y0, x1, y1 = w * 0.5, h / 2 + t / 2, w - t / 2, h - vy
            quads.append([(x0 - t / 2, y0), (x0 + t / 2, y0), (x1 + t / 2, y1), (x1 - t / 2, y1)])
    return rects, quads


def render(items, box):
    """items: (text, x, y, w, h, t, hx, vy, slant, pitch); box: (x0, y0, x1, y1) of the canvas, design px"""
    X0, Y0, X1, Y1 = box
    img = Image.new('L', ((X1 - X0) * SS, (Y1 - Y0) * SS), 0)
    d = ImageDraw.Draw(img)
    for text, x, y, w, h, t, hx, vy, slant, pitch in items:
        for i, ch in enumerate(text):
            cx = x + pitch * i
            rects, quads = segments(ch, w, h, t, hx, vy)
            polys = [[(a, b), (c, b), (c, e), (a, e)] for a, b, c, e in rects] + quads
            for poly in polys:
                pts = [((cx + px + slant * (h - py) - X0) * SS, (y + py - Y0) * SS) for px, py in poly]
                d.polygon(pts, fill=255)
    a = np.asarray(img, dtype=float).reshape(Y1 - Y0, SS, X1 - X0, SS).mean(axis=(1, 3)) / 255.0
    return a


def coverage(path, box):
    I = np.asarray(Image.open(path).convert('RGB')).astype(float)
    X0, Y0, X1, Y1 = box
    c = (I[Y0:Y1, X0:X1, 0] - I[Y0:Y1, X0:X1, 2] - 8.0) / 210.0
    return np.clip(c, 0.0, 1.0)


def fit(path, box, layout, p0, names):
    """layout(p) -> items; p0: start values; returns the fitted values and the rms error"""
    target = coverage(path, box)
    f = lambda p: ((render(layout(p), box) - target) ** 2).mean()
    r = optimize.minimize(f, p0, method='Powell', options={'maxiter': 20000, 'xtol': 1e-3, 'ftol': 1e-7})
    r = optimize.minimize(f, r.x, method='Nelder-Mead', options={'maxiter': 20000, 'xatol': 1e-3, 'fatol': 1e-8})
    print('  rms %.4f' % np.sqrt(r.fun), '  '.join('%s %.2f' % (n, v) for n, v in zip(names, r.x)))
    return r.x


R = 'reference/hdelay/'


def main():
    which = sys.argv[1:] or ['big', 'note', 'bar', 'small', 'pp', 'dual']
    if 'big' in which:
        # the tempo (Host: "120") and the time (MS: "0375"): one size of digits
        print('big digits, 120 (host)')
        lay = lambda p: [('120', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8])]
        fit(R + 'st_1_hover.png', (362, 88, 424, 120), lay, [367, 93, 12.9, 22.8, 2.49, 2.27, 1.61, 0.17, 16.9],
            ['x', 'y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'pitch'])
        print('big digits, 0375 (ms)')
        fit(R + 'cl_1_after.png', (322, 88, 400, 120), lay and (lambda p: [('0375', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8])]),
            [327, 92, 13, 23, 2.4, 2.4, 1.3, 0.15, 17.5], ['x', 'y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'pitch'])
    if 'note' in which:
        # a stacked note: "1" over the dashes over the denominator, then T or D (1/64D and 1/8T, 1/8D fitted jointly)
        for img, den, suf in (('notes/n_8_hover.png', '64', 'D'), ('notes/n_24_hover.png', '8', 'T'), ('notes/n_26_hover.png', '8', 'D')):
            print('note', '1/' + den + suf)
            n = len(den)
            lay = lambda p, den=den, suf=suf, n=n: [
                ('1', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], 12.0),
                ('-' * n, p[8], p[9], p[2], p[3], p[4], p[5], p[6], p[7], p[11]),
                (den, p[10], 112.0, p[2], p[3], p[4], p[5], p[6], p[7], p[11]),
                (suf, p[12], p[13], p[2], p[3], p[4], p[5], p[6], p[7], 12.0)]
            fit(R + img, (296, 76, 350, 132), lay,
                [304 if n == 1 else 309, 81, 11, 16, 2.0, 2.0, 1.0, 0.15, 301, 97, 301, 12, 322 if n == 1 else 332, 97],
                ['1x', '1y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'dash x', 'dash y', 'den x', 'pitch', 'suf x', 'suf y'])
    if 'bar' in which:
        print('1BAR')
        lay = lambda p: [('1', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], 12.0),
                         ('BAR', p[8], p[9], p[10], p[11], p[4], p[5], p[6], p[7], p[12])]
        fit(R + 'notes/n_40_hover.png', (300, 92, 352, 120), lay, [302, 97, 11, 16, 2.0, 2.0, 1.0, 0.15, 314, 100, 9.5, 13, 10.2],
            ['1x', '1y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'BAR x', 'BAR y', 'BAR w', 'BAR h', 'BAR pitch'])
    if 'small' in which:
        print('BPM label')
        lay = lambda p: [('BPM', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8])]
        fit(R + 'st_1_hover.png', (394, 116, 424, 132), lay, [398, 120, 6, 8.5, 1.2, 1.2, 0.6, 0.15, 7.0],
            ['x', 'y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'pitch'])
    if 'pp' in which:
        print('ping pong time 0415')
        lay = lambda p: [('0415', p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8])]
        fit(R + 'cl_3_after.png', (296, 98, 360, 124), lay, [299, 102, 10.5, 17, 2.0, 2.0, 1.0, 0.15, 14],
            ['x', 'y', 'w', 'h', 't', 'hx', 'vy', 'slant', 'pitch'])
    if 'dual' in which:
        print('dual: L, R, notes 1/8D, 120, BPM')
        lay = lambda p: [('L', p[0], p[1], p[2], p[3], p[4], p[4], p[5], p[6], 7.0), ('R', p[7], p[1], p[2], p[3], p[4], p[4], p[5], p[6], 7.0),
                         ('120', p[8], p[9], p[10], p[11], p[12], p[12], p[13], p[6], p[14])]
        fit(R + 'dual_2_hover.png', (300, 74, 420, 116), lay, [309, 77, 6, 9, 1.3, 0.6, 0.15, 405, 347, 96, 10.5, 17, 2.0, 1.0, 13.5],
            ['Lx', 'Ly', 'w', 'h', 't', 'vy', 'slant', 'Rx', '120x', '120y', 'w', 'h', 't', 'vy', 'pitch'])



def letter_strokes(letter, q):
    """the start side's letter as straight strokes (design px, centre lines): q holds x0, y0, x1, y1 of each, in the
    order of STROKES"""
    return [((q[4 * k], q[4 * k + 1]), (q[4 * k + 2], q[4 * k + 3])) for k in range(len(STROKES[letter]))]


# the strokes as first read off the reference's pixels (pixel centres); the fit moves every end
STROKES = {'L': [(400.5, 101.6, 399.6, 107.4), (399.4, 109.6, 398.6, 116.4), (400.4, 116.5, 405.6, 116.5)],
           'R': [(400.4, 101.5, 405.6, 101.5), (399.5, 102.6, 398.6, 107.4), (407.5, 102.6, 406.6, 107.4),
                 (400.4, 108.5, 405.6, 108.5), (398.5, 110.6, 397.6, 116.4), (400.0, 110.5, 405.5, 116.5)]}


def stroke_cover(lines, width, box):
    X0, Y0, X1, Y1 = box
    img = Image.new('L', ((X1 - X0) * SS, (Y1 - Y0) * SS), 0)
    d = ImageDraw.Draw(img)
    w = max(1, int(round(width * SS)))
    for (ax, ay), (bx, by) in lines:
        pa, pb = ((ax - X0) * SS, (ay - Y0) * SS), ((bx - X0) * SS, (by - Y0) * SS)
        d.line([pa, pb], fill=255, width=w)
        for c in (pa, pb):                                 # square caps
            d.rectangle((c[0] - w / 2, c[1] - w / 2, c[0] + w / 2, c[1] + w / 2), fill=255)
    return np.asarray(img, dtype=float).reshape(Y1 - Y0, SS, X1 - X0, SS).mean(axis=(1, 3)) / 255.0


def render_box(p, box, letter=None, lq=None, ink=None):
    """Ping Pong's start box: an amber parallelogram (top y0 .. bottom y1, left x at the top xl, right xr, leaning
    `lean` px right per px up) with corners rounded to rc; the start side's letter dark in it, a core stroke in a
    fainter wider stroke (ink = core width, the wide stroke's width, its darkness)"""
    xl, xr, y0, y1, lean, rc = p
    X0, Y0, X1, Y1 = box
    img = Image.new('L', ((X1 - X0) * SS, (Y1 - Y0) * SS), 0)
    h = y1 - y0
    tmp = Image.new('L', (int((xr - xl + 4) * SS), int((h + 2) * SS)), 0)
    ImageDraw.Draw(tmp).rounded_rectangle((SS, SS, (xr - xl + 1) * SS, (h + 1) * SS), radius=rc * SS, fill=255)
    sh = tmp.transform((tmp.width + int(4 * SS), tmp.height), Image.AFFINE, (1, lean, -lean * (h + 2) * SS, 0, 1, 0),
                       resample=Image.BILINEAR)
    img.paste(sh, (int(round((xl - 1 - X0) * SS)), int(round((y0 - 1 - Y0) * SS))))
    a = np.asarray(img, dtype=float).reshape(Y1 - Y0, SS, X1 - X0, SS).mean(axis=(1, 3)) / 255.0
    if letter:
        lines = letter_strokes(letter, lq)
        core, wide = stroke_cover(lines, ink[0], box), stroke_cover(lines, ink[1], box)
        dark = 1.0 - (1.0 - np.clip(ink[2], 0.0, 1.0) * wide) * (1.0 - core)
        a = a * (1.0 - dark)
    return a


def fit_box():
    box = (386, 90, 420, 126)
    bp = [391.22, 412.82, 95.0, 122.0, 0.04, 2.10]
    print('ping pong start box: xl %.2f xr %.2f top %.2f bottom %.2f lean %.3f corner %.2f' % tuple(bp))
    ink = [1.23, 1.8, 0.4]
    for it in range(2):
        out = {}
        for letter, shot in (('L', 'p_4_hover.png'), ('R', 'p_16_hover.png')):
            t = coverage(R + 'ppdual/' + shot, box)
            q0 = [v for st in STROKES[letter] for v in st]
            f = lambda q: ((render_box(bp, box, letter, q, ink) - t) ** 2).mean()
            r = optimize.minimize(f, q0, method='Powell', options={'maxiter': 40000, 'xtol': 1e-3, 'ftol': 1e-9})
            out[letter] = (r.x, t)
            print('  %s rms %.4f strokes' % (letter, np.sqrt(r.fun)), '; '.join('%.2f %.2f %.2f %.2f' % tuple(r.x[4 * k:4 * k + 4])
                                                                             for k in range(len(STROKES[letter]))))
        g = lambda k: sum(((render_box(bp, box, l, out[l][0], k) - out[l][1]) ** 2).mean() for l in 'LR')
        r = optimize.minimize(g, ink, method='Nelder-Mead', options={'xatol': 1e-3, 'fatol': 1e-9})
        ink = list(r.x)
        print('  ink: core %.2f, wide %.2f at %.2f (rms %.4f)' % (ink[0], ink[1], ink[2], np.sqrt(r.fun / 2)))
        for l in 'LR': STROKES[l] = [tuple(out[l][0][4 * k:4 * k + 4]) for k in range(len(STROKES[l]))]


if __name__ == '__main__':
    if sys.argv[1:] == ['ppbox']: fit_box()
    else: main()
