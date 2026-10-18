"""Null test: WBass (offline renderer of the plug-in DSP) against the reference RBass render, residual in dB.

    python -I nulltest_wbass.py [--quick] [--out report.md] [key=value ...]     (keys go to WBassRender)

For every signal x setting both render the same stereo input at 48 kHz. The residual is
20 log10( rms(ours - reference) / rms(reference) ) after the first 0.2 s, for the whole output and, separately, for
the generated (harmonic) part alone: the reference's generated part is split off with a second render at Intensity
-24 dB (Intensity is an exact gain on it), so the bass enhancer itself is judged, not only the dry signal that the
original path passes. Numbers are reported as they come: no case is labelled a null unless the data shows it.
"""
import sys, os, subprocess, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, sosfilt
import refhost as R

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get("WBASS_RENDER", os.path.join(HERE, "..", "build", "plugins", "WBass", "Release", "WBassRender.exe"))
TMP = os.path.join(HERE, "renders"); os.makedirs(TMP, exist_ok=True)
FS = 48000


def ours(x, fs, freq, intensity, gain, orig_in, tune=()):
    x = np.atleast_2d(np.asarray(x, np.float32))
    ch, n = x.shape
    fi, fo = os.path.join(TMP, "bin%d.f32" % os.getpid()), os.path.join(TMP, "bout%d.f32" % os.getpid())
    x.T.astype(np.float32).tofile(fi)
    args = [EXE, fi, fo, str(ch), str(fs), str(freq), str(intensity), str(gain), "1" if orig_in else "0", *tune]
    subprocess.run(args, check=True, capture_output=True)
    return np.fromfile(fo, np.float32).reshape(-1, ch).T


def residual_db(a, b, skip=0):
    a = np.asarray(a, np.float64)[..., skip:]; b = np.asarray(b, np.float64)[..., skip:]
    num = np.sqrt(np.mean((a - b) ** 2)); den = np.sqrt(np.mean(b ** 2))
    return 20 * np.log10(max(num, 1e-12) / max(den, 1e-12))


def signals(fs=FS, quick=False):
    rng = np.random.default_rng(5)
    n = int(3.0 * fs); t = np.arange(n) / fs
    S = {}
    for f, L in ((30, -20), (50, -12), (80, -20), (120, -12), (40, -40), (60, -3)):
        S["sine %d Hz %d dBFS" % (f, L)] = R.tone(fs, f, 3.0, L)
    f0, f1 = 20.0, 2000.0
    S["log sweep 20-2k -20"] = (0.1 * np.sin(2 * np.pi * f0 * (3.0 / np.log(f1 / f0)) * (np.exp(t / 3.0 * np.log(f1 / f0)) - 1))).astype(np.float32)
    S["bass steps -50/-10/-30"] = np.concatenate([R.tone(fs, 55, 1.0, -50), R.tone(fs, 55, 1.0, -10), R.tone(fs, 55, 1.0, -30)])
    z = rng.standard_normal(n)
    S["pink-ish noise -20 rms"] = (sosfilt(butter(1, 200, fs=fs, output='sos'), z) * 0.4).astype(np.float32)
    S["white noise -20 rms"] = (z / z.std() * 0.1).astype(np.float32)
    # bass-guitar-like notes: decaying harmonic tones
    notes = np.zeros(n);
    for k, f in enumerate((41.2, 55.0, 73.4, 49.0, 61.7, 82.4)):
        s0 = int(k * 0.5 * fs); m = int(0.5 * fs); tt = np.arange(m) / fs
        notes[s0:s0 + m] += 0.5 * np.exp(-tt / 0.3) * sum(np.sin(2 * np.pi * f * h * tt) / h ** 1.5 for h in range(1, 8))
    S["bass notes"] = notes.astype(np.float32)
    # kick-like: pitch-dropping sine bursts
    kick = np.zeros(n)
    for k in range(6):
        s0 = int(k * 0.5 * fs); m = int(0.35 * fs); tt = np.arange(m) / fs
        kick[s0:s0 + m] += 0.8 * np.exp(-tt / 0.08) * np.sin(2 * np.pi * (45 * tt + 60 * 0.03 * (1 - np.exp(-tt / 0.03))))
    S["kick-like"] = kick.astype(np.float32)
    if quick:
        keep = ["sine 50 Hz -12 dBFS", "log sweep 20-2k -20", "bass steps -50/-10/-30", "pink-ish noise -20 rms", "bass notes", "kick-like"]
        S = {k: v for k, v in S.items() if k in keep}
    return S


# (Freq, Intensity, Gain, Original Bass In)
SETTINGS = [(80, 0, 0, False), (80, 0, 0, True), (80, 12, 0, False), (80, -12, 0, True), (40, 0, 0, False), (160, 0, 0, False),
            (256, 6, -6, True), (32, 0, 0, False)]


def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    tune = tuple(a for a in args if "=" in a and not a.startswith("--"))
    out_md = args[args.index("--out") + 1] if "--out" in args else None
    S = signals(quick=quick)
    settings = SETTINGS[:4] if quick else SETTINGS
    rows = []
    for (freq, inten, gain, orig_in) in settings:
        for name, x in S.items():
            xs = np.stack([x, x]).astype(np.float32)
            ref = R.render_rbass(xs, FS, freq=freq, intensity=inten, gain=gain, hp=0 if orig_in else 1)
            ref_lo = R.render_rbass(xs, FS, freq=freq, intensity=inten - 24 if inten - 24 >= -24 else -24, gain=gain, hp=0 if orig_in else 1)
            y = ours(xs, FS, freq, inten, gain, orig_in, tune)
            skip = int(0.2 * FS)
            tot = residual_db(y, ref, skip)
            # generated part alone (reference split off with the Intensity render; ours from a render at -inf intensity)
            g_hi = 10 ** (inten / 20); g_lo = 10 ** (max(inten - 24, -24) / 20)
            gen_ref = (ref.astype(np.float64) - ref_lo) / (1 - g_lo / g_hi)
            y0 = ours(xs, FS, freq, -200, gain, orig_in, tune)
            gen_ours = y.astype(np.float64) - y0
            gen = residual_db(gen_ours, gen_ref, skip)
            rows.append(dict(freq=freq, intensity=inten, gain=gain, orig_in=orig_in, signal=name, total_db=round(tot, 1), generated_db=round(gen, 1)))
            print("Freq %3d Int %+3d Gain %+3d %s  %-26s total %7.1f dB  generated %7.1f dB" % (freq, inten, gain, "In " if orig_in else "Out", name, tot, gen), flush=True)
    tots = np.array([r["total_db"] for r in rows]); gens = np.array([r["generated_db"] for r in rows])
    print("\nmedian total %.1f dB (worst %.1f), median generated %.1f dB (worst %.1f), %d cases" % (np.median(tots), tots.max(), np.median(gens), gens.max(), len(rows)))
    if out_md:
        with open(out_md, "w") as f:
            f.write("| Freq | Intensity | Gain | Original Bass | Signal | Total residual (dB) | Generated part (dB) |\n|---|---|---|---|---|---|---|\n")
            for r in rows:
                f.write("| %d | %+d | %+d | %s | %s | %.1f | %.1f |\n" % (r["freq"], r["intensity"], r["gain"], "In" if r["orig_in"] else "Out", r["signal"], r["total_db"], r["generated_db"]))
            f.write("\nMedian total %.1f dB (worst %.1f); median generated part %.1f dB (worst %.1f); %d cases.\n" % (np.median(tots), tots.max(), np.median(gens), gens.max(), len(rows)))
        json.dump(rows, open(os.path.splitext(out_md)[0] + ".json", "w"), indent=1)


if __name__ == "__main__":
    main()
