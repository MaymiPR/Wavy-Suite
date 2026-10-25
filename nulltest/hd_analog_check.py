"""Phase 3: is H-Delay's Analog stage H-Comp's? H-Comp's fitted model (measurements/hcomp/analog_model.json, Phase 4
of WComp) applied to H-Delay's Analog-Off output (Mix 0: the stage comes after the mix) against H-Delay's own modes."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import hdelay as H
import fit_hc_analog2 as A

m = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hcomp', 'analog_model.json')))
fs = 48000; n = fs * 2; t = np.arange(n) / fs
rng = np.random.default_rng(1)
for level in (1.0, 0.1):
    x = sum(a * np.sin(2 * np.pi * f * t + rng.uniform(0, 6)) for a, f in ((0.25, 110), (0.15, 440), (0.1, 1320), (0.05, 3500)))
    x = (level * x * np.minimum(1, t * 4)).astype(np.float32)
    y0 = H.render(np.stack([x, x]), fs, preroll=1, analog="Off", mix=0)[0]
    for mode in (1, 2, 3, 4):
        q = np.array(m['modes'][str(mode)])
        yr = H.render(np.stack([x, x]), fs, preroll=1, analog="Mod%d" % mode, mix=0)[0]
        ym = A.model(q, y0.astype(float))
        seg = slice(fs // 2, n - 1000)
        db = lambda e: 20 * np.log10(np.sqrt((e ** 2).mean()) / np.sqrt((yr[seg] ** 2).mean()))
        print('peaks %.2f, mode %d: null with H-Comp\'s model %.1f dB (no stage %.1f dB)' % (0.55 * level, mode, db(yr[seg] - ym[seg]), db(yr[seg] - y0[seg])))
