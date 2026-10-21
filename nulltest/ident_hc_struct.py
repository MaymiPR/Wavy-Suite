"""WComp Phase 4: search the structure of H-Comp's look-ahead gain path on spike and burst responses.

Candidates: h = max |x| over H look-ahead samples; gt = static gain(h); a one-pole (attack coefficient falling,
instant rising: short events) and an FIR made of two boxes, in either order, each in the linear or the dB domain.

    python -I ident_hc_struct.py
"""
import sys, os, itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.ndimage import maximum_filter1d, uniform_filter1d
import measure_hcomp as M
from hcmodel import static_gain_db

FS = 48000
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'hc_struct_cases.npz')


def cases():
    if os.path.exists(CACHE):
        z = np.load(CACHE, allow_pickle=True)
        return list(z['cases'])
    out = []
    at = 12000
    for att in (0.5, 1.0, 3.0, 7.0):
        for loud in (-20.0, -10.0, 0.0):
            for k in (1, 5, 20, 64, 150):
                a = np.full(FS // 2, 10 ** (-60 / 20)); a[at:at + k] = 10 ** (loud / 20)
                x = (a * (1 - 2 * (np.arange(len(a)) % 2))).astype(np.float32)
                g = M.gain_trace(x, M.hc(x, threshold=-30.0, ratio=50.0, attack=att, release=3.0, punch=0.0, output=-18.0)[0])
                out.append((att, loud, k, a[at - 300:at + 700].astype(np.float64), (g / np.median(g[:10000]))[at - 300:at + 700]))
    np.savez(CACHE, cases=np.array(out, dtype=object))
    return out


def onepole(gt, c, dom):
    v = 20 * np.log10(gt) if dom == 'db' else gt.copy()
    o = np.empty_like(v); s = v[0]
    for i in range(len(v)):
        s = v[i] if v[i] > s else v[i] + c * (s - v[i])
        o[i] = s
    return 10 ** (o / 20) if dom == 'db' else o


def boxes(g, b1, b2, dom):
    v = 20 * np.log10(g) if dom == 'db' else g.copy()
    for b in (b1, b2):
        if b > 1:
            # causal box: mean of the last b samples
            k = np.ones(b) / b
            v = np.convolve(np.concatenate([np.full(b - 1, v[0]), v]), k, mode='valid')
    return 10 ** (v / 20) if dom == 'db' else v


def predict(a, att, H, lead, b1, b2, order, d1, d2):
    # h[n] = max a[n+lead .. n+lead+H-1]
    pad = np.concatenate([a, np.full(H + lead, a[-1])])
    h = maximum_filter1d(pad, size=H, origin=-(H // 2))[lead:lead + len(a)] if H > 1 else pad[lead:lead + len(a)]
    gt = 10 ** (static_gain_db(h, -30.0, 50.0) / 20)
    gt /= gt[0]
    c = np.exp(-1.0 / (att * 1e-3 * FS))
    if order == 'pole-fir':
        return boxes(onepole(gt, c, d1), b1, b2, d2)
    return onepole(boxes(gt, b1, b2, d2), c, d1)


def score(cs, *p):
    e = []
    for att, loud, k, a, g in cs:
        e.append(np.abs(predict(a, att, *p) - g)[250:900].max())
    return max(e), np.mean(e)


def main():
    cs = cases()
    # check the hold helper's alignment
    best = []
    sub = [c for c in cs if c[2] in (1, 20, 150) and c[1] in (-10.0, 0.0)]
    for order in ('pole-fir', 'fir-pole'):
        for d1 in ('lin', 'db'):
            for d2 in ('lin', 'db'):
                for H in (32, 48, 63, 64, 65):
                    for lead in (0, 1, 2):
                        for b1 in (1, 16, 32, 48, 63, 64, 65):
                            b2_opts = [1] + [b for b in (16, 24, 27, 32, 38, 48, 64) if b <= b1]
                            for b2 in b2_opts:
                                if H + b1 + b2 < 100 or H + b1 + b2 > 140: continue
                                s = score(sub, H, lead, b1, b2, order, d1, d2)
                                best.append((s[0], s[1], order, d1, d2, H, lead, b1, b2))
    best.sort()
    for b in best[:15]: print('worst %.4f mean %.4f  %s pole:%s fir:%s H %d lead %d boxes %d,%d' % b)


if __name__ == '__main__':
    main()
