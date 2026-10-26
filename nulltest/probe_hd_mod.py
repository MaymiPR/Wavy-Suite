"""Probe: modulation nulls over Rate x Delay x Depth (ms mode, Feedback 0, Analog Off), voice-like signal.

    python -I probe_hd_mod.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wdelay as N

def case(x, fs, ms, depth, rate, fb=0, **kw):
    k = dict(analog="Off", feedback=0, depth=depth, rate=rate, preroll=3.0,
             changes=[(-2.5, "sync", "ms"), (-2.45, "delay", ms), (0.0, "feedback", fb)], **kw)
    r = N.theirs(x, fs, **k); o = N.ours(x, fs, **k)
    return o, r

if __name__ == "__main__":
    fs = 48000
    S = N.signals(fs); x = S[[k for k in S if 'voice' in k][0]]; x = np.stack([x, 0.8 * x])
    grid = [(ms, d, r) for ms in (30, 750) for d in (39, 100) for r in (1, 30, 300, 1000, 3074, 6000)]
    if len(sys.argv) > 1: grid = [tuple(map(float, a.split(','))) for a in sys.argv[1:]]
    for ms, d, rt in grid:
        o, r = case(x, fs, ms, d, rt)
        np.save(os.path.join(N.TMP, 'pm_%g_%g_%g.npy' % (ms, d, rt)), np.stack([o, r]))
        print('ms %5g depth %3g rate %6g: %6.1f dB' % (ms, d, rt, N.residual_db(o, r)), flush=True)
