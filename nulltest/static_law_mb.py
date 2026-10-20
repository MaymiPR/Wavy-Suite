"""WBassX identification, step 5: the static gain law g(level) on a grid of Freq, Ratio and tone frequency: for each
steady tone the constant gain that makes the identified structure reproduce the reference's fundamental.

    python -I static_law_mb.py F ratio out.json
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, scipy.signal as ss, scipy.optimize as so
import mbmodel as MM, measure_maxxbass as M

F, ratio, out = float(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
dec = -15.0
res = {}
for r0 in (0.15, 0.25, 0.4, 0.6, 0.8, 1.0, 1.5):
    f0 = round(F * r0)
    if f0 < 10: continue
    x1 = M.tone(f0, 0.5, 0.0).astype(np.float64); u1 = ss.sosfilt(MM.lp3(F), x1)
    row = {}
    for lv in range(-96, 1, 6):
        A = 10 ** (lv / 20)
        ref = M.harmonic_amps(M.mb(M.tone(f0, 1.2, lv), harmonics_gain=0.0, monitor=1, ratio=ratio, harmonics_decay=dec, frequency=F)[0], f0, secs=0.4)[0] / A
        def h1(gdb):
            G = -MM.k_out(F) * ss.sosfilt(MM.g_highpass(F, 1), MM.generator(10 ** (gdb / 20) * A * u1, F, dec))
            return M.db(M.harmonic_amps(G, f0, secs=0.2)[0] / A)
        try: row[lv] = round(so.brentq(lambda q: h1(q) - M.db(ref), -45, 3, xtol=1e-3), 3)
        except ValueError: row[lv] = None
    res[f0] = row
    print(F, ratio, f0, row, flush=True)
json.dump(res, open(out, "w"), indent=0)
