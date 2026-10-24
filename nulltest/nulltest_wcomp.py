"""Null test: WComp (offline renderer of the plug-in DSP) against the reference H-Comp, residual in dB. Analog Off
unless a setting names a mode (modes 1-3 add random noise, which no null can remove; mode 4 is noise-free).

    python -I nulltest_wcomp.py [--quick] [--out report.md] [--fs 48000]

Residual = 20 log10( rms(ours - reference) / rms(reference) ) after the first 0.2 s. Numbers are reported as they
come: no case is labelled a null unless the data shows it.
"""
import sys, os, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import nulltest_wvox as NV

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get("WCOMP_RENDER", os.path.join(HERE, "..", "build", "plugins", "WComp", "Release", "WCompRender.exe"))
TMP = os.path.join(HERE, "renders"); os.makedirs(TMP, exist_ok=True)

# (threshold, ratio, attack, release, punch, mix, output, limiter)
SETTINGS = [(-18.0, 3.0, 7.0, 100.0, 1.0, 100.0, 0.0, 'Off'),          # the default
            (-30.0, 4.0, 0.5, 50.0, 0.0, 100.0, -6.0, 'Off'),
            (-24.0, 10.0, 20.0, 300.0, 3.0, 100.0, -6.0, 'Off'),
            (-36.0, 2.0, 3.0, 1000.0, 1.0, 100.0, -12.0, 'Off'),
            (-12.0, 50.0, 1.0, 30.0, 10.0, 100.0, 0.0, 'Off'),
            (-24.0, 4.0, 7.0, 100.0, 1.0, 50.0, 0.0, 'Off'),
            (-18.0, 3.0, 7.0, 100.0, 1.0, 100.0, 12.0, 'Off'),            # the output limiter working
            (-18.0, 3.0, 7.0, 100.0, 1.0, 100.0, 12.0, 'On'),
            (-24.0, 4.0, 7.0, 100.0, 1.0, 100.0, 0.0, 'Off', 4),
            (-18.0, 3.0, 7.0, 100.0, 1.0, 100.0, 0.0, 'Off', 2),        # the default Analog
            (-24.0, 4.0, 7.0, 100.0, 1.0, 100.0, 0.0, 'Off', 1),
            (-24.0, 4.0, 7.0, 100.0, 1.0, 100.0, 0.0, 'Off', 3)]


def ours(x, fs, thr, rat, att, rel, pu, mix, out, lim, analog=0):
    x = np.atleast_2d(np.asarray(x, np.float32)); ch = x.shape[0]
    fi, fo = os.path.join(TMP, "cin%d.f32" % os.getpid()), os.path.join(TMP, "cout%d.f32" % os.getpid())
    x.T.astype(np.float32).tofile(fi)
    args = [EXE, fi, fo, str(ch), str(fs), "threshold=%g" % thr, "ratio=%g" % rat, "attack=%g" % att, "release=%g" % rel,
            "punch=%g" % pu, "mix=%g" % mix, "output=%g" % out, "limiter=%d" % (lim == 'On'), "analog=%d" % analog,
            "learned=%s" % os.environ.get("WCOMP_LEARNED", "1")]      # WCOMP_LEARNED=0: the analytical model alone
    subprocess.run(args, check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, ch).T


def theirs(x, fs, thr, rat, att, rel, pu, mix, out, lim, analog=0):
    return R.render_hc(x, fs, threshold=thr, ratio=rat, attack=att, release=rel, punch=pu, mix=mix, output=out, limiter=lim,
                       analog=analog or 'Off')


def signals(fs, quick=False):
    S = NV.signals(fs, quick=quick)
    rng = np.random.default_rng(5)
    n = int(4 * fs); t = np.arange(n) / fs
    # a drum-like loop: kick (decaying 55 Hz sine), snare (noise burst), hats (short noise ticks)
    d = np.zeros(n)
    for k in range(8):
        s0 = int(k * 0.5 * fs); m = int(0.25 * fs); tt = np.arange(m) / fs
        d[s0:s0 + m] += 0.8 * np.sin(2 * np.pi * 55 * tt * (1 + 2 * np.exp(-tt / 0.02))) * np.exp(-tt / 0.08)
        s1 = s0 + int(0.25 * fs)
        if s1 + m <= n: d[s1:s1 + m] += 0.4 * rng.standard_normal(m) * np.exp(-tt / 0.04)
        for h in range(4):
            s2 = s0 + int(h * 0.125 * fs); mh = int(0.02 * fs)
            if s2 + mh <= n: d[s2:s2 + mh] += 0.1 * rng.standard_normal(mh) * np.exp(-np.arange(mh) / (0.004 * fs))
    S["drum loop"] = d.astype(np.float32)
    # a bass line: a sawtooth with plucked notes
    b = np.zeros(n)
    for k in range(16):
        s0 = int(k * 0.25 * fs); m = int(0.24 * fs); tt = np.arange(m) / fs; f0 = [55, 55, 82.4, 73.4][k % 4]
        b[s0:s0 + m] = 0.5 * (2 * ((f0 * tt) % 1) - 1) * np.exp(-tt / 0.15)
    S["bass line"] = b.astype(np.float32)
    return S


def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    out_md = args[args.index("--out") + 1] if "--out" in args else None
    fs = int(args[args.index("--fs") + 1]) if "--fs" in args else 48000
    S = signals(fs, quick)
    settings = SETTINGS[:3] + SETTINGS[8:10] if quick else SETTINGS
    if "--analog" in args: settings = SETTINGS[8:]
    rows = []
    for st in settings:
        for name, x in S.items():
            xs = np.stack([x, x]).astype(np.float32)
            a, b = ours(xs, fs, *st), theirs(xs, fs, *st)
            res = NV.residual_db(a[0], b[0], int(0.2 * fs))
            rows.append((st, name, res))
            print("thr %5.1f r %4.1f att %5.1f rel %6.1f punch %4.1f mix %5.1f out %+5.1f lim %-3s analog %s  %-26s %7.1f dB" % (
                *st[:8], st[8] if len(st) > 8 else 'Off', name, res), flush=True)
    v = np.array([r[2] for r in rows])
    print("median residual %.1f dB, best %.1f, worst %.1f (%d cases)" % (np.median(v), v.min(), v.max(), len(v)))
    if out_md:
        with open(out_md, "w", encoding="utf-8") as f:
            f.write("# WComp null test results\n\nResidual = 20 log10(rms(WComp - reference) / rms(reference)), %d Hz, after the first 0.2 s. Analog Off unless stated; modes 1-3 add random noise, which no null can remove.\n\n" % fs)
            f.write("| threshold | ratio | attack | release | punch | mix | output | limiter | analog | signal | residual (dB) |\n|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|\n")
            for st, name, res in rows: f.write("| %g | %g | %g | %g | %g | %g | %g | %s | %s | %s | %.1f |\n" % (*st[:8], st[8] if len(st) > 8 else "Off", name, res))
            f.write("\nmedian %.1f dB, best %.1f, worst %.1f\n" % (np.median(v), v.min(), v.max()))


if __name__ == '__main__':
    main()
