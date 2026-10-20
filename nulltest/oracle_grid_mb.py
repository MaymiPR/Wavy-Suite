"""Oracle check on grid cases: the generated part re-rendered through the identified structure with the reference's own
gain (extracted, constrained), against the network's result. Tells which residual belongs to the learned level
control and which to the structure.

    python -I oracle_grid_mb.py grid.json [count] [k m]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wbass as NW, nulltest_wbassx as NX, extract_g3 as E3

E3.KIND = "prev"
rows = json.load(open(sys.argv[1]))
count = int(sys.argv[2]) if len(sys.argv) > 2 else 24
k, m = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 1)
S = NW.signals()
rng = np.random.default_rng(3)
pick = [rows[i] for i in rng.choice(len(rows), count, replace=False)][k::m]
for r in pick:
    x = S[r["signal"]][: 2 * 48000]
    st = (r["F"], r["ratio"], r["response"], r["decay"], r["hp"], float(r["orig"]), r["harm"])
    G = NX.theirs(np.stack([x, x]).astype(np.float32), 48000, *st, 1)[0].astype(np.float64)
    g, err = E3.extract(x.astype(np.float64), G, r["F"], r["hp"], r["decay"], outer=10, iters=300)
    print(json.dumps(dict(st=st, signal=r["signal"], network=r["gen"], oracle=round(err, 1))), flush=True)
