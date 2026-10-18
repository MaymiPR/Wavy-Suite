"""Phase 3 measurements of the reference RBass (private analysis; results are not published).

    python -I measure_rbass.py [section ...]   sections: latency linear harmonics transfer dynamics response gain stereo smoothing

Results go to <repo>/measurements/rbass/ (git-ignored): results.json and png plots.
"""
import sys, os, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import refhost as R

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass")
os.makedirs(OUT, exist_ok=True)
FS = 48000
RES_PATH = os.path.join(OUT, "results.json")
RES = json.load(open(RES_PATH)) if os.path.exists(RES_PATH) else {}


def save(): json.dump(RES, open(RES_PATH, "w"), indent=1)
def fig(name): plt.tight_layout(); plt.savefig(os.path.join(OUT, name), dpi=110); plt.close()
def db(v): return 20 * np.log10(np.maximum(np.abs(v), 1e-12))


def components(y, f0, nh=10, secs=0.5):
    """Complex amplitude of the harmonics 1..nh of f0 over the last `secs` (whole periods, Hann window)."""
    per = FS / f0
    n = int(round(int(secs * f0) * per))
    seg = np.asarray(y[-n:], np.float64)
    t = np.arange(n) / FS
    w = np.hanning(n); w /= w.sum() / 2
    return np.array([np.sum(seg * w * np.exp(-2j * np.pi * f0 * h * t)) for h in range(1, nh + 1)])


def tone(f, secs, lvl): return R.tone(FS, f, secs, lvl)


# ---------------------------------------------------------------- latency
def sec_latency():
    out = {}
    for fs in (44100, 48000, 96000, 192000):
        p = R.load("RBass Stereo")
        x = np.zeros(int(0.3 * fs), np.float32); x[int(0.1 * fs)] = 0.5
        y = R.render_rbass(x, fs, freq=80, intensity=-24, hp=0)[0]
        out[str(fs)] = {"reported": int(p.reported_latency_samples), "impulse_peak_offset": int(np.argmax(np.abs(y))) - int(0.1 * fs)}
    RES["latency"] = out
    print("latency", out)


# ---------------------------------------------------------------- linear part: gain at the fundamental
def sec_linear():
    freqs = np.geomspace(20, 20000, 40)
    out = {}
    plt.figure(figsize=(9, 5))
    for fset in (40, 80, 160, 256):
        for hp in (0, 1):
            for inten in (-24, 0):
                g = []
                for f in freqs:
                    x = tone(f, 1.0, -30)
                    y = R.render_rbass(x, FS, freq=fset, intensity=inten, hp=hp)[0]
                    g.append(float(db(components(y, f, 1)[0]) - db(components(x, f, 1)[0])))
                key = f"freq{fset}_hp{hp}_int{inten}"
                out[key] = g
                if inten == 0: plt.semilogx(freqs, g, label=f"Freq {fset}, Original Bass {'out' if hp else 'in'}")
        print("linear", fset, flush=True)
    plt.xlabel("frequency (Hz)"); plt.ylabel("gain at the fundamental (dB)"); plt.grid(alpha=.3, which="both"); plt.legend(fontsize=7)
    plt.title("RBass: response at the input frequency (-30 dBFS tones, Intensity 0)")
    fig("linear.png")
    RES["linear"] = {"freqs": freqs.tolist(), **out}


# ---------------------------------------------------------------- harmonics
def sec_harmonics():
    out = {}
    f0s = [25, 30, 40, 50, 60, 80, 100, 120, 160, 200, 300, 500]
    for fset in (80, 160):
        for inten in (-12, 0, 12):
            rows = []
            for f0 in f0s:
                x = tone(f0, 1.2, -20)
                y = R.render_rbass(x, FS, freq=fset, intensity=inten, hp=1)[0]
                c = components(y, f0); cin = components(x, f0, 1)[0]
                rows.append({"f0": f0, "amp_db_re_input": (db(c) - db(cin)).tolist(),
                             "phase_deg_re_fund_input": np.degrees(np.angle(c / cin ** np.arange(1, 11) * abs(cin) ** np.arange(1, 11))).tolist()})
            out[f"freq{fset}_int{inten}"] = rows
            print("harmonics", fset, inten, flush=True)
    RES["harmonics"] = out
    plt.figure(figsize=(9, 5))
    for key in ("freq80_int0", "freq160_int0"):
        rows = out[key]
        for h in (2, 3, 4, 5):
            plt.semilogx([r["f0"] for r in rows], [r["amp_db_re_input"][h - 1] for r in rows], label=f"{key} H{h}")
    plt.xlabel("input frequency (Hz)"); plt.ylabel("harmonic level re input (dB)"); plt.grid(alpha=.3, which="both"); plt.legend(fontsize=6)
    plt.title("RBass: generated harmonics (Original Bass out, -20 dBFS tone)")
    fig("harmonics.png")


# ---------------------------------------------------------------- level transfer of the generated harmonics
def sec_transfer():
    out = {}
    levels = np.arange(-60, 0.1, 3.0)
    plt.figure(figsize=(9, 5))
    for f0, fset in ((50, 80), (100, 160)):
        rows = []
        for L in levels:
            x = tone(f0, 1.2, L)
            y = R.render_rbass(x, FS, freq=fset, intensity=0, hp=1)[0]
            c = components(y, f0)
            tot = np.sqrt(np.sum(np.abs(c[1:]) ** 2))
            rows.append({"level": float(L), "fund_db": float(db(c[0])), "harm_total_db": float(db(tot)), "h_db": db(c).tolist()})
        out[f"f{f0}_freq{fset}"] = rows
        plt.plot(levels, [r["harm_total_db"] for r in rows], label=f"{f0} Hz tone, Freq {fset}: harmonics")
        plt.plot(levels, [r["fund_db"] for r in rows], "--", label=f"{f0} Hz tone, Freq {fset}: fundamental")
    plt.plot(levels, levels - 3.01, "k:", lw=0.7)
    plt.xlabel("input peak (dBFS)"); plt.ylabel("output component (dB re full scale)"); plt.grid(alpha=.3); plt.legend(fontsize=7)
    plt.title("RBass: harmonic level vs input level (Intensity 0, Original Bass out)")
    fig("transfer.png")
    RES["transfer"] = out
    print("transfer done")


# ---------------------------------------------------------------- dynamics of the generator
def env(y, f0):
    per = int(round(FS / f0)); m = len(y) // per
    return np.sqrt(2 * np.mean(np.asarray(y[: m * per], np.float64).reshape(m, per) ** 2, axis=1)), per / FS


def sec_dynamics():
    out = {}
    plt.figure(figsize=(9, 5))
    for lo, hi in ((-50, -10), (-40, -20)):
        x = np.concatenate([tone(50, 0.6, lo), tone(50, 1.0, hi), tone(50, 1.5, lo)])
        y = R.render_rbass(x, FS, freq=80, intensity=0, hp=1)[0]
        e_in, dt = env(x, 50); e_out, _ = env(y, 50)
        g = db(e_out) - db(e_in)
        t = np.arange(len(g)) * dt
        out[f"{lo}_{hi}"] = {"t": t.tolist(), "gain_db": g.tolist()}
        plt.plot(t, g, label=f"50 Hz {lo} -> {hi} -> {lo} dB")
    plt.xlabel("time (s)"); plt.ylabel("output / input (dB, per period)"); plt.grid(alpha=.3); plt.legend()
    plt.title("RBass: gain of the processed signal through level steps (Freq 80, Original Bass out)")
    fig("dynamics.png")
    RES["dynamics"] = out
    print("dynamics done")


# ---------------------------------------------------------------- the hidden Response / Ratio / Decay / Input
def sec_response():
    out = {}
    x = np.concatenate([tone(50, 0.6, -50), tone(50, 1.0, -10), tone(50, 1.5, -50)])
    for name, idx, vals in (("Response", 2, (0, 5, 15, 30)), ("Ratio", 3, (0.25, 0.63, 1.0)), ("Decay", 8, (0, 1))):
        rows = {}
        for v in vals:
            y = R.render_rbass(x, FS, freq=80, intensity=0, hp=1, extra={idx: v})[0]
            rows[str(v)] = {"steady_db": float(R.rms_db(y[int(1.2 * FS):int(1.55 * FS)]) - R.rms_db(x[int(1.2 * FS):int(1.55 * FS)])),
                            "after_200ms_db": float(R.rms_db(y[int(1.8 * FS):int(1.85 * FS)]) - R.rms_db(x[int(1.8 * FS):int(1.85 * FS)]))}
        out[name] = rows
        print(name, rows, flush=True)
    RES["hidden"] = out


# ---------------------------------------------------------------- gain and clipping
def sec_gain():
    out = {}
    for gn in (0, -6, -12, -30):
        x = tone(1000, 0.6, -20)
        y = R.render_rbass(x, FS, freq=80, intensity=0, hp=0, gain=gn)[0]
        out[f"gain{gn}"] = float(R.rms_db(y[FS // 4:]) - R.rms_db(x[FS // 4:]))
    clip = []
    for L in (-12, -6, -3, 0, 3, 6):
        for inten in (0, 24):
            x = tone(50, 1.0, L)
            y = R.render_rbass(x, FS, freq=80, intensity=inten, hp=0)[0]
            clip.append({"in_db": L, "intensity": inten, "out_peak_db": float(db(np.abs(y[FS // 2:]).max()))})
    out["clip"] = clip
    RES["gain"] = out
    print("gain", json.dumps(out)[:500])


# ---------------------------------------------------------------- stereo
def sec_stereo():
    L = tone(50, 1.2, -10); Rr = tone(70, 1.2, -40)
    y = R.render_rbass(np.stack([L, Rr]), FS, freq=120, intensity=0, hp=1)
    cl = components(y[0], 50); cr = components(y[1], 70)
    yl = R.render_rbass(np.stack([L, 0 * Rr]), FS, freq=120, intensity=0, hp=1)
    yr = R.render_rbass(np.stack([0 * L, Rr]), FS, freq=120, intensity=0, hp=1)
    ym = R.render_rbass(L, FS, freq=120, intensity=0, hp=1, name="RBass Mono")
    RES["stereo"] = {"L_h2_together": float(db(cl[1])), "L_h2_alone": float(db(components(yl[0], 50)[1])),
                     "R_h2_together": float(db(cr[1])), "R_h2_alone": float(db(components(yr[1], 70)[1])),
                     "R_leak_when_only_L": float(R.rms_db(yl[1])), "mono_component_L_h2": float(db(components(ym[0], 50)[1]))}
    print("stereo", RES["stereo"])


# ---------------------------------------------------------------- parameter smoothing
def sec_smoothing():
    p = R.load("RBass Stereo")
    R.render_rbass(tone(1000, 0.1, -20), FS, freq=80, intensity=0, hp=0)   # set the state
    x = np.stack([tone(1000, 1.0, -20)] * 2)
    blk = 64; y = np.zeros_like(x); first = True
    for i in range(0, x.shape[1], blk):
        if i == int(0.5 * FS) // blk * blk: p.out_gain = -12.0
        y[:, i:i + blk] = p(x[:, i:i + blk], FS, buffer_size=blk, reset=first); first = False
    e, dt = env(y[0], 1000); g = db(e) - (-20)
    i0 = int(0.5 / dt)
    frac = (g[i0 - 2:i0 + 200] - g[i0 - 5]) / (-12.0)
    t = (np.arange(len(frac)) - 2) * dt * 1000
    RES["smoothing"] = {"gain_step_t50_ms": float(t[np.argmax(frac >= 0.5)]), "gain_step_t90_ms": float(t[np.argmax(frac >= 0.9)])}
    print("smoothing", RES["smoothing"])


SECTIONS = {"latency": sec_latency, "linear": sec_linear, "harmonics": sec_harmonics, "transfer": sec_transfer,
            "dynamics": sec_dynamics, "response": sec_response, "gain": sec_gain, "stereo": sec_stereo, "smoothing": sec_smoothing}

if __name__ == "__main__":
    for s in sys.argv[1:] or list(SECTIONS):
        t0 = time.time(); SECTIONS[s](); save(); print(f"[{s}] {time.time() - t0:.1f}s", flush=True)
