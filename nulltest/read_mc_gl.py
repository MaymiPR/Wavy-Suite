"""Reads MeterCapture's gl=1 output (out.raw + .txt + .pcm) into an npz for vu_angles.py: t (audio time when the
frame was drawn, s), wall, meter (n, h, w, 3), audio (2, n) the plug-in's output.

    python -I read_mc_gl.py <out.raw> <out.npz>
"""
import sys
import numpy as np

raw, out = sys.argv[1], sys.argv[2]
w, h = map(int, open(raw + '.txt').read().split())
rec = np.dtype([('t', '<f8'), ('wall', '<f8'), ('px', np.uint8, (h, w, 3))])
d = np.fromfile(raw, rec)
pcm = np.fromfile(raw + '.pcm', np.float32).reshape(-1, 2).T
np.savez(out, t=d['t'], wall=d['wall'], meter=d['px'], audio=pcm)
print('%d frames, %.1f fps, %dx%d' % (len(d), len(d) / max(d['wall'][-1], 1e-9), w, h))
