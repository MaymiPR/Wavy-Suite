"""Phase 3 measurements of the reference RVox (private analysis, nothing here is published).

    python -I measure_rvox.py [section ...]       sections: map latency static dynamics detector
                                                    freq thd smoothing gate stereo   (default: all)

Results go to <repo>/measurements/rvox/ (git-ignored): results.json, csv tables and png plots.
"""
import sys, os, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import refhost as R

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rvox")
os.makedirs(OUT, exist_ok=True)
FS = 48000
RES_PATH = os.path.join(OUT, "results.json")
RES = json.load(open(RES_PATH)) if os.path.exists(RES_PATH) else {}


def save():
    json.dump(RES, open(RES_PATH, "w"), indent=1)


def csv(name, header, rows):
    np.savetxt(os.path.join(OUT, name), np.asarray(rows, float), delimiter=",", header=header, comments="", fmt="%.6g")


def fig(name):
    plt.tight_layout()
    plt.savefig(os.path.join(OUT, name), dpi=110)
    plt.close()


def db(v):
    return 20 * np.log10(np.maximum(np.abs(v), 1e-12))


def env_db(y, fs, win_ms=1.0):  # win_ms in milliseconds
    """Peak envelope in dB over non-overlapping windows (1 ms = one cycle of 1 kHz)."""
    n = int(fs * win_ms / 1000)
    m = len(y) // n
    return db(np.abs(y[: m * n]).reshape(m, n).max(axis=1)), n / fs


def settled_rms_db(y, fs, tail=0.4):
    return R.rms_db(y[-int(tail * fs):])


# ---------------------------------------------------------------- map
def sec_map():
    p = R.load()
    rows = {}
    for name in ("compression", "gain", "gate"):
        par = p.parameters[name]
        vals = []
        grid = np.linspace(0, 1, 11)
        for g in grid:
            par.raw_value = float(g)
            vals.append((float(g), par.string_value))
        rows[name] = vals
    RES["map"] = rows
    print("map done")


# ---------------------------------------------------------------- latency
def sec_latency():
    """pedalboard compensates the plug-in's reported latency, so the residual impulse offset is ~0 when the
    report is truthful; the report itself is the number to compare with the product page (64/64/128/256)."""
    out = {}
    for fs in (44100, 48000, 88200, 96000, 176400, 192000):
        for comp in (0.0, -20.0):
            x = np.zeros(int(0.25 * fs), np.float32)
            x[int(0.05 * fs)] = 0.05
            y = R.render(x, fs, comp=comp)[0]
            resid = int(np.argmax(np.abs(y))) - int(0.05 * fs)
            out[f"{fs}_comp{comp:g}"] = {"reported": int(R.load().reported_latency_samples), "residual_after_compensation": resid}
    RES["latency_samples"] = out
    print("latency", out)


# ---------------------------------------------------------------- static transfer curve
def sec_static():
    levels = np.arange(-80, 0.1, 2.0)       # peak dBFS of a 1 kHz sine
    comps = [0, -6, -12, -18, -24, -30, -36]
    table = {}
    plt.figure(figsize=(8, 6))
    for comp in comps:
        outl = []
        for L in levels:
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=comp)[0]
            outl.append(settled_rms_db(y, FS) - settled_rms_db(x, FS))
        table[comp] = outl
        plt.plot(levels, levels + np.array(outl), label=f"comp {comp} dB")
    plt.plot(levels, levels, "k--", lw=0.6)
    plt.xlabel("input peak (dBFS)"); plt.ylabel("output (dB re. 1 kHz sine peak)"); plt.grid(alpha=.3); plt.legend()
    plt.title("RVox static transfer, gate off, gain 0")
    fig("static_transfer.png")
    csv("static_gain.csv", "input_dB," + ",".join(f"comp{c}" for c in comps),
        np.column_stack([levels] + [table[c] for c in comps]))
    RES["static"] = {"levels": levels.tolist(), "gain_db": {str(c): table[c] for c in comps}}

    # fine sweep near the knee for two comp settings + slope
    fine = np.arange(-70, 0.1, 0.5)
    fine_tab = {}
    for comp in (-12, -24):
        g = []
        for L in fine:
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=comp)[0]
            g.append(settled_rms_db(y, FS) - settled_rms_db(x, FS))
        fine_tab[comp] = g
    plt.figure(figsize=(8, 5))
    for comp, g in fine_tab.items():
        out = fine + np.array(g)
        slope = np.gradient(out, fine)
        plt.plot(fine, slope, label=f"comp {comp} dB  (local slope d out / d in)")
    plt.xlabel("input peak (dBFS)"); plt.ylabel("slope"); plt.grid(alpha=.3); plt.legend()
    plt.title("Knee shape: local slope of the static curve")
    fig("knee_slope.png")
    RES["knee_fine"] = {"levels": fine.tolist(), "gain_db": {str(k): v for k, v in fine_tab.items()}}

    # gain (limiter ceiling): comp -20, gain settings
    gains = [0, -6, -12, -24]
    lv = np.arange(-40, 0.1, 2.0)
    gtab = {}
    plt.figure(figsize=(8, 5))
    for gn in gains:
        o = []
        for L in lv:
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=-20, gain=gn)[0]
            o.append(float(np.abs(y[-int(0.4 * FS):]).max()))
        gtab[gn] = o
        plt.plot(lv, db(np.array(o)), label=f"gain {gn} dB")
    plt.xlabel("input peak (dBFS)"); plt.ylabel("output peak (dBFS)"); plt.grid(alpha=.3); plt.legend()
    plt.title("Output ceiling (comp -20 dB)")
    fig("ceiling.png")
    RES["ceiling_peak_db"] = {"levels": lv.tolist(), **{str(k): db(np.array(v)).tolist() for k, v in gtab.items()}}
    print("static done")


# ---------------------------------------------------------------- attack / release
def step_signal(fs, lo, hi, t_lo=0.6, t_hi=1.2, t_end=1.8, f=1000.0):
    return np.concatenate([R.tone(fs, f, t_lo, lo), R.tone(fs, f, t_hi, hi), R.tone(fs, f, t_end, lo)])


def times_to(g, t, t0, thr_list):
    """time after t0 at which g has covered fractions thr of its move from g(t0-) to its settled value."""
    i0 = int(round(t0 / t[1]))
    g0 = np.median(g[i0 - 30: i0 - 5])
    seg_end = g[i0 + int(0.5 / t[1]) if False else min(len(g) - 1, i0 + 200)]
    return g0


def sec_dynamics():
    """Step responses. 12 kHz (8-sample = 0.167 ms envelope) resolves the attack; 1 kHz (1 ms envelope) the release."""
    out = {}
    cases = [(-12, -50, -10), (-24, -50, -10), (-24, -50, -30), (-36, -50, -10)]
    fig_, axs = plt.subplots(2, 1, figsize=(9, 8))
    for ax, (f, win) in zip(axs, ((1000.0, 1.0), (12000.0, 8000.0 / FS))):
        for comp, lo, hi in cases:
            x = step_signal(FS, lo, hi, f=f)
            y = R.render(x, FS, comp=comp, preroll=0.2)[0]
            ei, dt = env_db(x, FS, win); eo, _ = env_db(y, FS, win)
            g = eo - ei
            t = np.arange(len(g)) * dt
            res = {}
            for name, t0, tend in (("attack", 0.6, 1.75), ("release", 1.8, 3.55)):
                i0 = int(round(t0 / dt))
                nb = int(0.02 / dt)
                g_before = float(np.median(g[i0 - nb: i0 - 2]))
                g_after = float(np.median(g[int(round(tend / dt)) - nb: int(round(tend / dt)) - 2]))
                span = g_after - g_before
                r = {"gain_before_db": g_before, "gain_after_db": g_after}
                if abs(span) > 0.3:
                    frac = (g[i0 - 2: i0 + int(1.5 / dt)] - g_before) / span
                    tt = (np.arange(len(frac)) - 2) * dt * 1000.0
                    for q in (0.1, 0.5, 0.632, 0.9, 0.99):
                        idx = np.where(frac >= q)[0]
                        r[f"t{int(q*1000)}_ms"] = float(tt[idx[0]]) if len(idx) else None
                res[name] = r
            out[f"{int(f)}Hz_comp{comp}_{lo}_{hi}"] = res
            ax.plot(t, g, label=f"comp {comp}, {lo}->{hi} dB")
        ax.set_title(f"Step response, {int(f)} Hz test tone"); ax.set_xlabel("time (s)"); ax.set_ylabel("gain (dB)"); ax.grid(alpha=.3); ax.legend(fontsize=7)
    fig("dynamics_steps.png")
    RES["dynamics"] = out
    for k, v in out.items():
        print(k, {n: {q: (round(z, 2) if isinstance(z, float) else z) for q, z in d.items()} for n, d in v.items()})


# ---------------------------------------------------------------- detector: peak vs RMS, burst length
def sec_detector():
    out = {}
    comp = -24
    amp = 10 ** (-20 / 20)
    n = int(1.6 * FS)
    t = np.arange(n) / FS
    sine = amp * np.sin(2 * np.pi * 1000 * t)
    sq_peak = amp * np.sign(np.sin(2 * np.pi * 1000 * t))               # same peak as the sine (+3 dB RMS)
    sq_rms = sq_peak * 10 ** (-3.0103 / 20)                             # same RMS as the sine
    rng = np.random.default_rng(1)
    noise = rng.standard_normal(n); noise *= amp / np.sqrt(2) / noise.std()   # same RMS as the sine, high crest
    cases = {"sine": sine, "square_same_peak": sq_peak, "square_same_rms": sq_rms, "noise_same_rms": noise}
    for k, x in cases.items():
        y = R.render(x.astype(np.float32), FS, comp=comp)[0]
        out[k] = {"in_rms_db": R.rms_db(x[-FS // 2:]), "in_peak_db": float(db(np.abs(x).max())),
                  "gain_db": settled_rms_db(y, FS) - R.rms_db(x[-FS // 2:])}
    # burst length vs. final gain reduction
    bl = {}
    for ms in (1, 3, 10, 30, 100, 300):
        x = np.concatenate([np.zeros(int(0.1 * FS)), R.tone(FS, 1000, ms / 1000, -10), np.zeros(int(0.3 * FS))]).astype(np.float32)
        y = R.render(x, FS, comp=comp)[0]
        a = int(0.1 * FS); b = a + int(ms / 1000 * FS)
        pk_in = float(np.abs(x[a:b]).max()); pk_out = float(np.abs(y[a:b + 100]).max())
        bl[str(ms)] = float(db(pk_out) - db(pk_in))
    out["burst_peak_gain_db"] = bl
    RES["detector"] = out
    print("detector", json.dumps(out)[:700])


# ---------------------------------------------------------------- frequency response
def sec_freq():
    freqs = np.geomspace(20, 20000, 31)
    out = {}
    plt.figure(figsize=(8, 5))
    for label, comp, lvl in (("comp 0, -30 dB", 0, -30), ("comp -24, -50 dB", -24, -50), ("comp -24, -20 dB", -24, -20)):
        g = []
        for f in freqs:
            x = R.tone(FS, f, 1.5, lvl)
            y = R.render(x, FS, comp=comp)[0]
            g.append(settled_rms_db(y, FS, 0.5) - settled_rms_db(x, FS, 0.5))
        out[label] = g
        plt.semilogx(freqs, g, label=label)
    plt.xlabel("frequency (Hz)"); plt.ylabel("gain (dB)"); plt.grid(alpha=.3, which="both"); plt.legend()
    plt.title("Frequency response (steady tones)")
    fig("freq_response.png")
    csv("freq_response.csv", "freq," + ",".join(out.keys()).replace(",", ";").replace(";", ","), np.column_stack([freqs] + list(out.values())))
    RES["freq"] = {"freqs": freqs.tolist(), **out}
    print("freq done")


# ---------------------------------------------------------------- THD / harmonics
def harmonics(y, fs, f0, nh=10):
    n = int(round(fs / f0)) * int(0.3 * f0)            # whole cycles
    seg = y[-n:] * np.blackman(n)
    S = np.abs(np.fft.rfft(seg))
    bins = [int(round(f0 * h * n / fs)) for h in range(1, nh + 1)]
    amp = [S[b - 2: b + 3].max() for b in bins]
    return np.array(amp)


def sec_thd():
    cfgs = [("comp 0 / gain 0", 0, 0), ("comp -20 / gain 0", -20, 0), ("comp -36 / gain 0", -36, 0), ("comp -20 / gain -12", -20, -12)]
    levels = [-40, -20, -10, -3, 0]
    out = {}
    plt.figure(figsize=(8, 5))
    for label, comp, gn in cfgs:
        thd = []
        h_table = []
        for L in levels:
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=comp, gain=gn)[0]
            a = harmonics(y.astype(np.float64), FS, 1000.0)
            thd.append(20 * np.log10(np.sqrt(np.sum(a[1:] ** 2)) / a[0] + 1e-15))
            h_table.append((20 * np.log10(a[1:] / a[0] + 1e-15)).tolist())
        out[label] = {"thd_db": thd, "harmonics_dbc_2_to_10": h_table}
        plt.plot(levels, thd, "o-", label=label)
    plt.xlabel("input peak (dBFS)"); plt.ylabel("THD (dB re. fundamental)"); plt.grid(alpha=.3); plt.legend()
    plt.title("THD of a 1 kHz sine")
    fig("thd.png")
    RES["thd"] = {"levels": levels, **out}
    print("thd", {k: [round(v, 1) for v in d["thd_db"]] for k, d in out.items()})


# ---------------------------------------------------------------- parameter smoothing
def block_render(p, x, fs, changes, block=64):
    """Render in small blocks; changes: {sample_index: {param: value}} applied before that block."""
    y = np.zeros_like(x)
    first = True
    pend = sorted(changes.items())
    for i in range(0, x.shape[1], block):
        while pend and pend[0][0] <= i:
            for k, v in pend.pop(0)[1].items(): setattr(p, k, float(v))
        y[:, i:i + block] = p(x[:, i:i + block], fs, buffer_size=block, reset=first)
        first = False
    return y


def sec_smoothing():
    p = R.load()
    out = {}
    plt.figure(figsize=(9, 5))
    n = int(2.0 * FS)
    x = R.tone(FS, 12000, 2.0, -20)[None, :]
    t_change = int(0.8 * FS)
    for pname, a, b in (("compression", 0.0, -12.0), ("gain", 0.0, -12.0), ("gate", -80.0, -10.0)):
        R.setp(p, 0.0, None, 0.0)
        p.compression = 0.0
        ch = {0: {pname: a}, t_change: {pname: b}}
        if pname == "gate": ch[0] = {"gate": float("-inf")}
        y = block_render(p, x, FS, ch)[0]
        e, dt = env_db(y, FS, 8000.0 / FS)
        ei, _ = env_db(x[0], FS, 8000.0 / FS)
        g = e - ei
        t = np.arange(len(g)) * dt
        i0 = int(t_change / FS / dt)
        before = float(np.median(g[i0 - 60: i0 - 5])); after = float(np.median(g[-60:-5]))
        span = after - before
        fr = (g[i0: i0 + 600] - before) / span if abs(span) > 0.3 else np.zeros(600)
        tt = np.arange(len(fr)) * dt * 1000
        res = {"before_db": before, "after_db": after}
        for f in (0.1, 0.5, 0.9, 0.99):
            idx = np.where(fr >= f)[0]
            res[f"t{int(f*100)}_ms"] = float(tt[idx[0]]) if len(idx) else None
        out[pname] = res
        plt.plot(t, g, label=f"{pname} {a:g} -> {b:g}")
    plt.xlabel("time (s)"); plt.ylabel("gain (dB)"); plt.grid(alpha=.3); plt.legend(); plt.title("Parameter step: gain trajectory (64-sample blocks)")
    fig("smoothing.png")
    RES["smoothing"] = out
    print("smoothing", out)


# ---------------------------------------------------------------- gate
def sec_gate():
    out = {}
    levels = np.arange(-90, -5, 2.0)
    plt.figure(figsize=(8, 5))
    for th in (-60, -40, -20):
        o = []
        for L in levels:
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=0, gate=th)[0]
            o.append(settled_rms_db(y, FS) - settled_rms_db(x, FS))
        out[f"gate{th}"] = o
        plt.plot(levels, o, label=f"gate {th} dB")
    plt.xlabel("input peak (dBFS)"); plt.ylabel("gain (dB)"); plt.grid(alpha=.3); plt.legend()
    plt.title("Gate (downward expander): gain vs level, comp 0")
    fig("gate_static.png")
    RES["gate"] = {"levels": levels.tolist(), **out}
    # gate release: level step down across the threshold
    x = step_signal(FS, -50, -10)
    y = R.render(x, FS, comp=0, gate=-30, preroll=0.2)[0]
    ei, dt = env_db(x, FS); eo, _ = env_db(y, FS)
    g = eo - ei
    t_dn = 1.8
    i0 = int(t_dn / dt)
    before = float(np.median(g[i0 - 40: i0 - 5]))
    seg = g[i0: i0 + int(1.0 / dt)]
    after = float(np.median(seg[-100:]))
    fr = (seg - before) / (after - before)
    tt = np.arange(len(fr)) * dt * 1000
    rel = {"before_db": before, "after_db": after}
    for f in (0.5, 0.632, 0.9):
        idx = np.where(fr >= f)[0]
        rel[f"t{int(f*1000)}_ms"] = float(tt[idx[0]]) if len(idx) else None
    # gate attack: step up
    i1 = int(0.6 / dt)
    b1 = float(np.median(g[i1 - 40: i1 - 5])); a1 = float(np.median(g[i1 + int(0.3 / dt): i1 + int(0.5 / dt)]))
    fr1 = (g[i1: i1 + int(0.5 / dt)] - b1) / (a1 - b1) if abs(a1 - b1) > 0.3 else None
    att = {"before_db": b1, "after_db": a1}
    if fr1 is not None:
        tt1 = np.arange(len(fr1)) * dt * 1000
        for f in (0.5, 0.632, 0.9):
            idx = np.where(fr1 >= f)[0]
            att[f"t{int(f*1000)}_ms"] = float(tt1[idx[0]]) if len(idx) else None
    RES["gate_dynamics"] = {"release": rel, "attack": att, "hidden_param_gate_release_default_ms": 130}
    print("gate", RES["gate_dynamics"])


# ---------------------------------------------------------------- stereo linking
def sec_stereo():
    L = R.tone(FS, 1000, 1.6, -10); Rr = R.tone(FS, 1300, 1.6, -40)
    x = np.stack([L, Rr])
    y = R.render(x, FS, comp=-24, name="RVox Stereo")
    gL = settled_rms_db(y[0], FS) - settled_rms_db(L, FS)
    gR = settled_rms_db(y[1], FS) - settled_rms_db(Rr, FS)
    ym = R.render(L, FS, comp=-24, name="RVox Mono")[0]
    gM = settled_rms_db(ym, FS) - settled_rms_db(L, FS)
    gLonly = settled_rms_db(R.render(np.stack([L, 0 * L]), FS, comp=-24, name="RVox Stereo")[0], FS) - settled_rms_db(L, FS)
    RES["stereo"] = {"L_-10dB_with_R_-40dB": gL, "R_-40dB_with_L_-10dB": gR, "mono_component_L_-10dB": gM, "stereo_L_only": gLonly}
    print("stereo", RES["stereo"])


# ---------------------------------------------------------------- gate knee, stereo link, static summary
def sec_gatefine():
    lv = np.arange(-48, -31.9, 0.5)
    out = {}
    for th in (-40, -20):
        g = []
        for L in lv + (th + 40):
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=0, gate=th)[0]
            g.append(settled_rms_db(y, FS) - settled_rms_db(x, FS))
        out[f"gate{th}"] = {"rel_level_db": (lv + 40).tolist(), "gain_db": g}
    RES["gate_fine"] = out
    print("gatefine", [round(v, 2) for v in out["gate-40"]["gain_db"]])


def sec_stereo2():
    L = R.tone(FS, 1000, 1.6, -40); Rr = R.tone(FS, 1300, 1.6, -10)
    y = R.render(np.stack([L, Rr]), FS, comp=-24, name="RVox Stereo")
    both = {"L_-40dB_with_R_-10dB": settled_rms_db(y[0], FS) - settled_rms_db(L, FS),
            "R_-10dB_with_L_-40dB": settled_rms_db(y[1], FS) - settled_rms_db(Rr, FS)}
    alone = {"L_-40dB_alone": settled_rms_db(R.render(np.stack([L, 0 * L]), FS, comp=-24, name="RVox Stereo")[0], FS) - settled_rms_db(L, FS),
             "R_-10dB_alone": settled_rms_db(R.render(np.stack([0 * Rr, Rr]), FS, comp=-24, name="RVox Stereo")[1], FS) - settled_rms_db(Rr, FS)}
    RES["stereo_link"] = {"together": both, "alone": alone}
    print("stereo link", RES["stereo_link"])


def sec_summary():
    st = RES["static"]
    lv = np.array(st["levels"])
    rows = {}
    for c, G in st["gain_db"].items():
        G = np.array(G); c = float(c)
        out = lv + G
        slope = np.gradient(out, lv)
        low = G[0]
        i3 = int(np.argmax(G <= low - 3.0)) if np.any(G <= low - 3.0) else -1
        rows[c] = {"gain_at_-80dB": float(G[0]), "gain_at_0dB": float(G[-1]),
                   "level_where_gain_is_3dB_below_low_level_gain": float(lv[i3]) if i3 >= 0 else None,
                   "slope_at_-20dB": float(slope[np.argmin(abs(lv + 20))]), "slope_at_-10dB": float(slope[np.argmin(abs(lv + 10))]),
                   "slope_at_-2dB": float(slope[np.argmin(abs(lv + 2))])}
    RES["static_summary"] = rows
    for c, r in rows.items(): print(c, {k: (round(v, 3) if v is not None else None) for k, v in r.items()})


def sec_limiter():
    """Over-range input: does the Gain control act as a ceiling? Peak of the output vs input and gain, and the THD that limiting causes."""
    out = {}
    for gn in (0, -6, -12):
        rows = []
        for L in (-6, 0, 3, 6, 12):
            x = R.tone(FS, 1000, 1.6, L)
            y = R.render(x, FS, comp=0, gain=gn)[0]
            a = harmonics(y.astype(np.float64), FS, 1000.0)
            rows.append({"in_peak_db": L, "out_peak_db": float(db(np.abs(y[-FS // 2:]).max())),
                         "thd_db": float(20 * np.log10(np.sqrt(np.sum(a[1:] ** 2)) / a[0] + 1e-15))})
        out[f"gain{gn}"] = rows
    # impulse train, comp 0: ceiling on transients
    x = np.zeros(FS, np.float32); x[::4800] = 2.0
    y = R.render(x, FS, comp=0, gain=0)[0]
    out["impulses_peak_2.0_in_out_peak"] = float(np.abs(y).max())
    RES["limiter"] = out
    for k, v in out.items(): print(k, v)


SECTIONS = {"map": sec_map, "latency": sec_latency, "static": sec_static, "dynamics": sec_dynamics, "detector": sec_detector,
            "freq": sec_freq, "thd": sec_thd, "smoothing": sec_smoothing, "gate": sec_gate, "stereo": sec_stereo,
            "gatefine": sec_gatefine, "stereo2": sec_stereo2, "summary": sec_summary, "limiter": sec_limiter}

if __name__ == "__main__":
    want = sys.argv[1:] or list(SECTIONS)
    for s in want:
        t0 = time.time()
        SECTIONS[s]()
        save()
        print(f"[{s}] {time.time() - t0:.1f}s", flush=True)
