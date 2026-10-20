"""Training set for the WBassX learned gain: reference renders of the generated part G (MaxxBass monitor) and the
level-control gain g(t) extracted from them with the identified structure (extract_g2.py).

    python -I render_trainset_mb.py split count seed [noextract]   -> measurements/maxxbass/train/<split>_<seed>_<i>.npz
    (noextract: the reference render only, for the end-to-end training; g is left empty)
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import render_trainset as RT
import extract_g2 as E

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "maxxbass", "train")
os.makedirs(OUT, exist_ok=True)

if __name__ == "__main__":
    split, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    noext = len(sys.argv) > 4 and sys.argv[4] == "noextract"
    rng = np.random.default_rng(seed)
    for i in range(count):
        F = int(round(np.exp(rng.uniform(np.log(32), np.log(256)))))
        ratio = 1.0 if rng.random() < float(os.environ.get("MB_R1P", "0.2")) else round(float(np.exp(rng.uniform(0, np.log(4)))), 2)
        response = 20.0 if rng.random() < 0.25 else round(float(rng.uniform(10, 30)), 1)
        decay = round(float(rng.uniform(-24, -9)), 1)
        hp = int(rng.choice([0, 1, 1, 2]))
        kind, x = RT.make(rng, F)
        fn = os.path.join(OUT, "%s_%d_%04d.npz" % (split, seed, i))
        if os.path.exists(fn): continue          # resume (the draws above keep the sequence the same)
        G = R.render_mb(x, FS, harmonics_gain=0.0, monitor=1, frequency=F, ratio=ratio, response=response,
                        harmonics_decay=decay, highpass=hp)[0].astype(np.float64)
        if noext: g, err = np.zeros(1), 0.0
        else: g, err = E.extract(x.astype(np.float64), G, F, hp, decay, outer=3, iters=150)
        np.savez_compressed(fn, x=x, G=G.astype(np.float32), g=g.astype(np.float32),
                            F=F, ratio=ratio, response=response, decay=decay, hp=hp, kind=kind, err=err)
        print(split, seed, i, kind, F, ratio, response, decay, hp, "re-render %.1f dB" % err, flush=True)
