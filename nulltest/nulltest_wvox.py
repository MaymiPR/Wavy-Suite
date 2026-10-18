"""Null test: WVox (offline renderer of the plug-in DSP) against the reference render, residual in dB.

    python -I nulltest_wvox.py [--quick] [--tune key=value ...] [--out report.md]

For every signal x setting both plug-ins render the same input at 48 kHz. The residual is
20 log10( rms(ours - reference) / rms(reference) ), measured after the first 0.2 s (start-up) and
also over the last 1 s (steady state). A residual of -60 dB or lower is a close match; 0 dB means nothing in common.
Numbers are reported as they come: no case is labelled a null unless the data shows it.
"""
import sys, os, subprocess, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get("WVOX_RENDER", os.path.join(HERE, "..", "build", "Release", "WVoxRender.exe"))
TMP = os.path.join(HERE, "renders"); os.makedirs(TMP, exist_ok=True)
FS = 48000


def ours(x, fs, comp, gate, gain, tune=()):
    x = np.atleast_2d(np.asarray(x, np.float32))
    ch, n = x.shape
    fi, fo = os.path.join(TMP, "in%d.f32" % os.getpid()), os.path.join(TMP, "out%d.f32" % os.getpid())
    x.T.astype(np.float32).tofile(fi)
    args = [EXE, fi, fo, str(ch), str(fs), str(comp), str(-80.0 if gate is None else gate), str(gain), *tune]
    subprocess.run(args, check=True, capture_output=True)
    y = np.fromfile(fo, np.float32).reshape(-1, ch).T
    return y


def residual_db(a, b, skip=0, end=None):
    a = np.asarray(a, np.float64)[..., skip:end]; b = np.asarray(b, np.float64)[..., skip:end]
    num = np.sqrt(np.mean((a - b) ** 2)); den = np.sqrt(np.mean(b ** 2))
    return 20 * np.log10(max(num, 1e-12) / max(den, 1e-12))


def signals(fs=FS, quick=False):
    rng = np.random.default_rng(11)
    n = int(3.0 * fs); t = np.arange(n) / fs
    S = {}
    for L in (-40, -20, -6):
        S[f"sine 1 kHz {L} dBFS"] = R.tone(fs, 1000, 3.0, L)
    S["sine 100 Hz -20"] = R.tone(fs, 100, 3.0, -20)
    S["sine 8 kHz -20"] = R.tone(fs, 8000, 3.0, -20)
    f0, f1 = 40.0, 16000.0
    S["log sweep -20"] = (10 ** (-20 / 20) * np.sin(2 * np.pi * f0 * (3.0 / np.log(f1 / f0)) * (np.exp(t / 3.0 * np.log(f1 / f0)) - 1))).astype(np.float32)
    S["tone steps -50/-10/-30"] = np.concatenate([R.tone(fs, 1000, 1.0, -50), R.tone(fs, 1000, 1.0, -10), R.tone(fs, 1000, 1.0, -30)])
    S["AM 5 Hz 3 kHz"] = (0.1 * (1 + 0.8 * np.sin(2 * np.pi * 5 * t)) * np.sin(2 * np.pi * 3000 * t)).astype(np.float32)
    z = rng.standard_normal(n); S["white noise -20 rms"] = (z / z.std() * 10 ** (-20 / 20)).astype(np.float32)
    # pink noise (Voss-McCartney style via FFT shaping)
    Z = np.fft.rfft(rng.standard_normal(n)); fr = np.fft.rfftfreq(n, 1 / fs); Z[1:] /= np.sqrt(fr[1:]); Z[0] = 0
    p = np.fft.irfft(Z, n); S["pink noise -20 rms"] = (p / p.std() * 10 ** (-20 / 20)).astype(np.float32)
    # drum-like: decaying noise bursts every 0.5 s
    d = np.zeros(n)
    for k in range(6):
        s0 = int(k * 0.5 * fs); m = int(0.3 * fs)
        d[s0:s0 + m] += rng.standard_normal(m) * np.exp(-np.arange(m) / (0.05 * fs)) * 0.5
    S["noise bursts"] = d.astype(np.float32)
    # voice-like: harmonic tone with a syllable envelope
    env = np.clip(np.sin(2 * np.pi * 3.5 * t), 0, None) ** 2
    voice = sum(np.sin(2 * np.pi * 140 * k * t) / k for k in range(1, 14)) * env * 0.25
    S["voice-like"] = voice.astype(np.float32)
    if quick:
        keep = ["sine 1 kHz -20 dBFS", "log sweep -20", "tone steps -50/-10/-30", "white noise -20 rms", "voice-like"]
        S = {k: v for k, v in S.items() if k in keep}
    return S


SETTINGS = [(0, None, 0), (-12, None, 0), (-24, None, 0), (-36, None, 0), (-24, -40, 0), (-24, None, -6), (-12, -30, -3)]


def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    tune = tuple(a for a in args if "=" in a and not a.startswith("--"))
    out_md = None
    if "--out" in args: out_md = args[args.index("--out") + 1]
    S = signals(quick=quick)
    rows = []
    print("%-26s %-22s %9s %9s %9s" % ("signal", "comp/gate/gain", "all", "steady", "peak err"))
    for name, x in S.items():
        for comp, gate, gain in SETTINGS:
            ref = R.render(x, FS, comp=comp, gate=gate, gain=gain)[0]
            mine = ours(x, FS, comp, gate, gain, tune)[0]
            all_ = residual_db(mine, ref, int(0.2 * FS)); st = residual_db(mine, ref, len(x) - FS)
            pk = 20 * np.log10(max(np.abs(mine - ref).max(), 1e-12) / max(np.abs(ref).max(), 1e-12))
            rows.append((name, f"{comp}/{'off' if gate is None else gate}/{gain}", all_, st, pk))
            print("%-26s %-22s %9.1f %9.1f %9.1f" % rows[-1])
    if "--extra" in args: extra_cases(rows)
    allv = np.array([r[2] for r in rows])
    print("median residual %.1f dB, best %.1f, worst %.1f; %d of %d cases below -60 dB" % (np.median(allv), allv.min(), allv.max(), (allv < -60).sum(), len(allv)))
    if out_md:
        with open(out_md, "w", encoding="utf-8") as f:
            f.write("# WVox null test results\n\nResidual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after the first 0.2 s. ")
            f.write("'steady' = last second only. Lower is better; about -60 dB or less is a close match, 0 dB is no match.\n\n")
            f.write("| signal | comp / gate / gain (dB) | residual all (dB) | residual steady (dB) | peak error (dB re. peak) |\n|---|---|---:|---:|---:|\n")
            for r in rows: f.write("| %s | %s | %.1f | %.1f | %.1f |\n" % r)
            f.write("\nMedian %.1f dB, best %.1f dB, worst %.1f dB; %d of %d cases below -60 dB.\n" % (np.median(allv), allv.min(), allv.max(), (allv < -60).sum(), len(allv)))
    return rows


def extra_cases(out_rows):
    """Other sample rates and stereo (linked) material."""
    rng = np.random.default_rng(5)
    print("-- other sample rates (mono)")
    for fs in (44100, 96000, 192000):
        n = int(2.0 * fs); t = np.arange(n) / fs
        z = rng.standard_normal(n); noise = (z / z.std() * 10 ** (-20 / 20)).astype(np.float32)
        steps = np.concatenate([R.tone(fs, 1000, 0.7, -50), R.tone(fs, 1000, 0.7, -10), R.tone(fs, 1000, 0.6, -30)])
        for name, x in ((f"sine 1 kHz -20 @{fs}", R.tone(fs, 1000, 2.0, -20)), (f"tone steps @{fs}", steps), (f"white noise @{fs}", noise)):
            for comp, gate, gain in ((-24, None, 0), (-12, -40, -3)):
                ref = R.render(x, fs, comp=comp, gate=gate, gain=gain)[0]
                mine = ours(x, fs, comp, gate, gain)[0]
                r = residual_db(mine, ref, int(0.2 * fs)); st = residual_db(mine, ref, len(x) - fs // 2)
                row = (name, f"{comp}/{'off' if gate is None else gate}/{gain}", r, st, 20 * np.log10(max(np.abs(mine - ref).max(), 1e-12) / max(np.abs(ref).max(), 1e-12)))
                out_rows.append(row); print("%-26s %-22s %9.1f %9.1f %9.1f" % row)
    print("-- stereo (linked), 48 kHz")
    n = int(3.0 * FS); t = np.arange(n) / FS
    zl = rng.standard_normal(n); zr = rng.standard_normal(n)
    st_sigs = {
        "stereo: L -10 / R -40 tones": np.stack([R.tone(FS, 1000, 3.0, -10), R.tone(FS, 1300, 3.0, -40)]),
        "stereo: noise L -20 / R -30": np.stack([zl / zl.std() * 10 ** (-20 / 20), zr / zr.std() * 10 ** (-30 / 20)]).astype(np.float32),
        "stereo: alternating loud channel": np.stack([R.tone(FS, 1000, 3.0, -15) * ((t // 0.5) % 2 == 0), R.tone(FS, 700, 3.0, -15) * ((t // 0.5) % 2 == 1)]).astype(np.float32),
    }
    for name, x in st_sigs.items():
        for comp, gate, gain in ((-24, None, 0), (-12, -40, -3)):
            ref = R.render(x, FS, comp=comp, gate=gate, gain=gain, name="RVox Stereo")
            mine = ours(x, FS, comp, gate, gain)
            r = residual_db(mine, ref, int(0.2 * FS)); st = residual_db(mine, ref, n - FS)
            row = (name, f"{comp}/{'off' if gate is None else gate}/{gain}", r, st, 20 * np.log10(max(np.abs(mine - ref).max(), 1e-12) / max(np.abs(ref).max(), 1e-12)))
            out_rows.append(row); print("%-26s %-22s %9.1f %9.1f %9.1f" % row)


if __name__ == "__main__":
    main()
