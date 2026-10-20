"""Null grid for WBassX: random settings over every control (seeded), a few signals each, residual in dB against the
reference, with a breakdown by parameter range, so weak regions show up.

    python -I nullgrid_wbassx.py [count] [--out grid.json]

Settings: Freq log-uniform 32 ... 256 (whole Hz), Ratio 1 (25 %) or log-uniform 1 ... 4, Response 10 ... 30 ms,
Decay -24 ... -9 dB, High-pass 6 / 12 / 24 dB/oct, Original Bass -inf / -6 / 0 / +6 dB, Harmonics -6 / 0 / +6 dB.
Signals: bass notes, kick-like, 50 Hz tone at -12 dBFS, pink-ish noise (nulltest_wbass.py signals).
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wbass as NW
import nulltest_wbassx as NX

NINF = float("-inf")


def settings(count, seed=11):
    rng = np.random.default_rng(seed)
    out = []
    for _ in range(count):
        F = int(round(np.exp(rng.uniform(np.log(32), np.log(256)))))
        ratio = 1.0 if rng.random() < 0.25 else round(float(np.exp(rng.uniform(0, np.log(4)))), 2)
        out.append((F, ratio, round(float(rng.uniform(10, 30)), 1), round(float(rng.uniform(-24, -9)), 1), int(rng.integers(0, 3)),
                    float(rng.choice([NINF, -6.0, 0.0, 6.0])), float(rng.choice([-6.0, 0.0, 6.0]))))
    return out


def main():
    args = sys.argv[1:]
    count = int(args[0]) if args and args[0].isdigit() else 120
    out = args[args.index("--out") + 1] if "--out" in args else None
    S = NW.signals()
    sig = {k: S[k] for k in ("bass notes", "kick-like", "sine 50 Hz -12 dBFS", "pink-ish noise -20 rms")}
    rows = []
    for st in settings(count):
        for name, x in sig.items():
            xs = np.stack([x, x]).astype(np.float32)
            tot = NW.residual_db(NX.ours(xs, 48000, *st, 0), NX.theirs(xs, 48000, *st, 0), 9600)
            gen = NW.residual_db(NX.ours(xs, 48000, *st, 1), NX.theirs(xs, 48000, *st, 1), 9600)
            rows.append(dict(F=st[0], ratio=st[1], response=st[2], decay=st[3], hp=st[4], orig=st[5], harm=st[6], signal=name,
                             total=round(tot, 1), gen=round(gen, 1)))
        print(st, [(r["signal"][:6], r["total"], r["gen"]) for r in rows[-4:]], flush=True)
    t = np.array([r["total"] for r in rows]); g = np.array([r["gen"] for r in rows])
    print("\n%d cases: total median %.1f (p90 %.1f, worst %.1f); generated median %.1f (p90 %.1f, worst %.1f)"
          % (len(rows), np.median(t), np.percentile(t, 90), t.max(), np.median(g), np.percentile(g, 90), g.max()))
    def brk(key, edges):
        v = np.array([r[key] for r in rows])
        for lo, hi in zip(edges[:-1], edges[1:]):
            m = (v >= lo) & (v < hi)
            if m.sum(): print(f"  {key} [{lo}, {hi}): n {m.sum():3d}  generated median {np.median(g[m]):6.1f}  p90 {np.percentile(g[m], 90):6.1f}")
    brk("F", [32, 50, 80, 128, 200, 257]); brk("ratio", [1, 1.001, 1.5, 2.5, 4.01]); brk("decay", [-24, -20, -16, -12, -8.9]); brk("hp", [0, 1, 2, 3])
    for name in sig:
        m = np.array([r["signal"] == name for r in rows]); print(f"  {name:24s} generated median {np.median(g[m]):6.1f}  p90 {np.percentile(g[m], 90):6.1f}")
    if out: json.dump(rows, open(out, "w"), indent=0, default=str)


if __name__ == "__main__":
    main()
