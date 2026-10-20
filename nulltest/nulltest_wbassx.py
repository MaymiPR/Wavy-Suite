"""Null test: WBassX (offline renderer of the plug-in DSP) against the reference MaxxBass render, residual in dB.

    python -I nulltest_wbassx.py [--quick] [--out report.md] [--fs 48000]

For every signal x setting both render the same stereo input. The residual is
20 log10( rms(ours - reference) / rms(reference) ) after the first 0.2 s, for the whole output (Monitor: Audio) and
for the generated part alone (Monitor: Harmonics, which plays it pre-fader on both). Numbers are reported as they
come: no case is labelled a null unless the data shows it.
"""
import sys, os, subprocess, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import nulltest_wbass as NW

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get("WBASSX_RENDER", os.path.join(HERE, "..", "build", "plugins", "WBassX", "Release", "WBassXRender.exe"))
TMP = os.path.join(HERE, "renders"); os.makedirs(TMP, exist_ok=True)
NINF = float("-inf")

# (Freq, Ratio, Response, Decay, High-pass 0/1/2, Original Bass dB, Harmonics dB)
SETTINGS = [(80, 1.0, 20.0, -15.0, 1, 0.0, 0.0), (80, 4.0, 20.0, -15.0, 1, NINF, 0.0), (80, 2.0, 10.0, -9.0, 2, NINF, 6.0),
            (50, 1.0, 30.0, -24.0, 0, 0.0, -6.0), (160, 1.41, 20.0, -15.0, 1, 0.0, 0.0), (256, 4.0, 15.0, -12.0, 1, -6.0, 3.0),
            (32, 1.0, 20.0, -15.0, 1, 0.0, 12.0), (120, 2.5, 25.0, -18.0, 2, NINF, 0.0)]


def ours(x, fs, F, ratio, resp, decay, hp, orig, harm, monitor):
    x = np.atleast_2d(np.asarray(x, np.float32)); ch = x.shape[0]
    fi, fo = os.path.join(TMP, "xin%d.f32" % os.getpid()), os.path.join(TMP, "xout%d.f32" % os.getpid())
    x.T.astype(np.float32).tofile(fi)
    db = lambda v: "-200" if v == NINF else "%g" % v
    args = [EXE, fi, fo, str(ch), str(fs), "freq=%g" % F, "ratio=%g" % ratio, "response=%g" % resp, "decay=%g" % decay,
            "highpass=%d" % hp, "orig=" + db(orig), "harm=" + db(harm), "monitor=%d" % monitor]
    subprocess.run(args, check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, ch).T


def theirs(x, fs, F, ratio, resp, decay, hp, orig, harm, monitor):
    return R.render_mb(x, fs, frequency=F, ratio=ratio, response=resp, harmonics_decay=decay, highpass=hp,
                       orig_bass_gain=orig, harmonics_gain=harm, monitor=monitor)


def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    out_md = args[args.index("--out") + 1] if "--out" in args else None
    fs = int(args[args.index("--fs") + 1]) if "--fs" in args else 48000
    S = NW.signals(fs, quick=quick)
    settings = SETTINGS[:4] if quick else SETTINGS
    rows = []
    for st in settings:
        for name, x in S.items():
            xs = np.stack([x, x]).astype(np.float32)
            skip = int(0.2 * fs)
            tot = NW.residual_db(ours(xs, fs, *st, 0), theirs(xs, fs, *st, 0), skip)
            gen = NW.residual_db(ours(xs, fs, *st, 1), theirs(xs, fs, *st, 1), skip)
            F, ratio, resp, decay, hp, orig, harm = st
            rows.append(dict(freq=F, ratio=ratio, response=resp, decay=decay, highpass=hp, orig=orig, harm=harm, signal=name,
                             total_db=round(tot, 1), generated_db=round(gen, 1)))
            print("F %3d R %.2f Resp %2.0f Dec %+3.0f HP %d Orig %5s Harm %+4.0f  %-26s total %7.1f dB  generated %7.1f dB"
                  % (F, ratio, resp, decay, hp, "-inf" if orig == NINF else "%+.0f" % orig, harm, name, tot, gen), flush=True)
    tots = np.array([r["total_db"] for r in rows]); gens = np.array([r["generated_db"] for r in rows])
    print("\nmedian total %.1f dB (worst %.1f), median generated %.1f dB (worst %.1f), %d cases"
          % (np.median(tots), tots.max(), np.median(gens), gens.max(), len(rows)))
    if out_md:
        with open(out_md, "w") as f:
            f.write("| Freq | Ratio | Response | Decay | High-pass | Original Bass | Harmonics | Signal | Total residual (dB) | Generated part (dB) |\n"
                    "|---|---|---|---|---|---|---|---|---|---|\n")
            for r in rows:
                f.write("| %d | %.2f | %.0f | %+.0f | %s | %s | %+.0f | %s | %.1f | %.1f |\n" % (
                    r["freq"], r["ratio"], r["response"], r["decay"], ("6", "12", "24")[r["highpass"]],
                    "-inf" if r["orig"] == NINF else "%+.0f" % r["orig"], r["harm"], r["signal"], r["total_db"], r["generated_db"]))
            f.write("\nMedian total %.1f dB (worst %.1f); median generated part %.1f dB (worst %.1f); %d cases.\n"
                    % (np.median(tots), tots.max(), np.median(gens), gens.max(), len(rows)))
        json.dump(rows, open(os.path.splitext(out_md)[0] + ".json", "w"), indent=1, default=str)


if __name__ == "__main__":
    main()
