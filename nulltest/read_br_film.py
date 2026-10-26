"""Reads a MeterCapture gl=1 film of Bass Rider (or WBass Ride's screen grab) into per-frame readings: the audio
time, the Energy meter's lit length (px of the 184 px bar, and the dB it shows on its -42 .. 0 scale), the Rider
cap's position (dB on the fader's scale, by matching its default frame), the clip LED, and the plug-in's own gain
over the same moment (output / input rms of the recorded audio, the 42 ms latency removed).

    python -I read_br_film.py <film.raw> <input mono f32> [out.npz] [--grab]   (--grab: MeterCapture's screen grab format)
"""
import sys, os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, '..', 'reference', 'wbassride', 'shot_default_start.png')


def frames(path, grab):
    w, h = map(int, open(path + '.txt').read().split())
    if grab: rec = np.dtype([('t', '<f8'), ('px', np.uint8, (h, w, 3))])
    else: rec = np.dtype([('t', '<f8'), ('wall', '<f8'), ('px', np.uint8, (h, w, 3))])
    return np.memmap(path, rec, 'r'), w, h


def main():
    a = sys.argv[1:]
    grab = '--grab' in a
    a = [x for x in a if not x.startswith('--')]
    path, inp = a[0], a[1]
    out = a[2] if len(a) > 2 else path + '.npz'
    d, w, h = frames(path, grab)
    sx = w / 320.0
    ref = np.asarray(Image.open(REF).convert('L')).astype(float)
    tmpl = ref[294:363, 140:180]                                  # the Rider cap at 0 dB (top 294)
    tmpl = tmpl - tmpl.mean()
    n = len(d)
    t = np.zeros(n); lit = np.zeros(n); rider = np.zeros(n); clip = np.zeros(n)
    for i in range(n):
        px = d['px'][i]
        if sx != 1.0:
            px = np.asarray(Image.fromarray(np.ascontiguousarray(px)).resize((320, 560), Image.BILINEAR))
        f = px.astype(float)
        t[i] = d['t'][i]
        row = f[56, 71:255]
        amber = (row[:, 0] > 180) & (row[:, 1] > 110) & (row[:, 2] < 110)
        lit[i] = np.max(np.where(amber)[0]) + 1 if amber.any() else 0
        g = f.mean(2)
        best, bo = -1e18, 0
        for off in range(-105, 106):                              # +-12 dB
            y0 = 294 + off
            if y0 < 170 or y0 + 69 > 500: continue
            win = g[y0:y0 + 69, 140:180]
            sc = float(np.sum((win - win.mean()) * tmpl))
            if sc > best: best, bo = sc, off
        rider[i] = -bo / 8.75
        clip[i] = 1.0 if (f[190, 287, 0] > 150 and f[190, 287, 1] < 90) else 0.0
    x = np.fromfile(inp, np.float32)
    pcm = np.fromfile(path + '.pcm', np.float32).reshape(-1, 2)[:, 0]
    lat = 2016
    gain = np.full(n, np.nan)
    win = 2400
    for i in range(n):
        k = int(t[i] * 48000)
        if k - win < 0 or k + lat >= len(pcm) or k >= len(x): continue
        xi = x[k - win:k]; yo = pcm[k - win + lat:k + lat]
        rx, ry = np.sqrt(np.mean(xi ** 2)), np.sqrt(np.mean(yo ** 2))
        if rx > 1e-4: gain[i] = 20 * np.log10(ry / rx)
    np.savez(out, t=t, lit=lit, meterDb=lit / 184.0 * 42.0 - 42.0, rider=rider, clip=clip, gain=gain)
    print('%d frames -> %s' % (n, out))


if __name__ == '__main__':
    main()
