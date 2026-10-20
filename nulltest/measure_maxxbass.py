"""Phase 3 measurements of the reference MaxxBass (private analysis; the rendered audio is not published).

    python -I measure_maxxbass.py [section ...]    sections: map latency linear input superpos harmonics level
                                                     dynamics response highpass stereo clip smoothing rbass
                                                     (default: all)

Results go to <repo>/measurements/maxxbass/: results.json, csv tables and png plots.
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import scipy.signal as ss
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import refhost as R

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "maxxbass")
os.makedirs(OUT, exist_ok=True)
FS = 48000
RES_PATH = os.path.join(OUT, "results.json")
RES = json.load(open(RES_PATH)) if os.path.exists(RES_PATH) else {}
NINF = float("-inf")


def save():
    json.dump(RES, open(RES_PATH, "w"), indent=1, default=float)


def csv(name, header, rows):
    np.savetxt(os.path.join(OUT, name), np.asarray(rows, float), delimiter=",", header=header, comments="", fmt="%.6g")


def fig(name):
    plt.tight_layout()
    plt.savefig(os.path.join(OUT, name), dpi=110)
    plt.close()


def db(v):
    return 20 * np.log10(np.maximum(np.abs(v), 1e-15))


def mb(x, **kw):
    return R.render_mb(x, FS, **kw)


def impulse(n=FS, at=1000, a=0.05):
    x = np.zeros(n, np.float32); x[at] = a
    return x


def harmonic_amps(y, f0, secs=0.5, nh=8):
    """Complex amplitudes of harmonics 1..nh of f0 over the last whole periods in the final `secs` of y."""
    per = FS / f0
    k = max(1, int(secs * f0))
    n = int(round(k * per))
    seg = np.asarray(y[-n:], np.float64)
    t = (np.arange(len(y) - n, len(y))) / FS
    return np.array([2 * np.mean(seg * np.exp(-2j * np.pi * h * f0 * t)) for h in range(1, nh + 1)])


def tone(f, secs, dbfs):
    return R.tone(FS, f, secs, dbfs)


# ---------------------------------------------------------------- bilinear Butterworth helpers (for the model checks)
def butter(order, f, kind):
    return ss.butter(order, f, kind, fs=FS)


def freqz(ba, freqs):
    return ss.freqz(*ba, worN=np.asarray(freqs, float), fs=FS)[1]


# ---------------------------------------------------------------- map
def sec_map():
    p = R.load("MaxxBass Stereo")
    rows = {}
    for name in ("input_gain", "orig_bass_gain", "harmonics_gain", "ratio", "response", "harmonics_decay", "frequency",
                 "highpass", "monitor"):
        par = p.parameters[name]
        vals = []
        for g in np.linspace(0, 1, 13):
            par.raw_value = float(g)
            vals.append((round(float(g), 4), par.string_value.strip()))
        rows[name] = vals
    R.set_mb(p)
    RES["map"] = rows
    print(json.dumps(rows, indent=0)[:1500])


# ---------------------------------------------------------------- latency
def sec_latency():
    out = {}
    for fs in (44100, 48000, 96000, 192000):
        for hg in (NINF, 0.0):
            x = np.zeros(int(0.5 * fs), np.float32); x[int(0.1 * fs)] = 0.3
            y = R.render_mb(x, fs, harmonics_gain=hg)[0]
            out[f"{fs}_harm{hg:g}"] = {"reported": int(R.load("MaxxBass Stereo").reported_latency_samples),
                                       "peak_at": int(np.argmax(np.abs(y))) - int(0.1 * fs)}
    RES["latency"] = out
    print(out)


# ---------------------------------------------------------------- linear paths (Harmonics -inf)
FREQS = np.geomspace(10, 20000, 400)


def sec_linear():
    """Impulse responses of the original path with the generated path off: Original Bass gain and Freq sweeps,
    the Original Bass monitor, linearity (two levels). Compared with HP3(F) + g LP3(F) (bilinear Butterworth)."""
    out = {}
    plt.figure(figsize=(9, 6))
    for F in (32, 80, 256):
        hp, lp = butter(3, F, "high"), butter(3, F, "low")
        for og in (0.0, -6.0, 12.0, NINF):
            for mon in (0, 2):
                h = mb(impulse(), frequency=F, orig_bass_gain=og, monitor=mon)[0][1000:] / 0.05
                H = np.fft.rfft(h, 1 << 17); fr = np.fft.rfftfreq(1 << 17, 1 / FS)
                g = 0.0 if og == NINF else 10 ** (og / 20)
                # Audio: HP3 + g LP3; the Original Bass monitor: LP3 alone, pre-fader
                model = freqz(hp, fr) + g * freqz(lp, fr) if mon == 0 else freqz(lp, fr)
                band = (fr > 5) & (fr < 20000)
                err = db(np.sqrt(np.mean(np.abs(H[band] - model[band]) ** 2) / np.mean(np.abs(model[band]) ** 2 + 1e-30)))
                key = f"F{F}_orig{og:g}_mon{mon}"
                mag = np.interp(FREQS, fr, db(H))
                out[key] = {"mag_db_at": {str(int(f)): round(float(np.interp(f, fr, db(H))), 2) for f in (20, F / 2, F, 2 * F, 1000)},
                            "model_HP3_plus_gLP3_err_db": round(float(err), 1)}
                if mon == 0 and og in (0.0, NINF):
                    plt.semilogx(FREQS, mag, label=key)
    # linearity
    a = mb(impulse(a=0.01))[0][1000:] / 0.01; b = mb(impulse(a=0.5))[0][1000:] / 0.5
    out["linearity_err_db"] = round(float(db(np.sqrt(np.sum((a - b) ** 2) / np.sum(a * a)))), 1)
    plt.ylim(-60, 15); plt.grid(True, which="both", alpha=0.3); plt.legend(fontsize=7); plt.xlabel("Hz"); plt.ylabel("dB")
    plt.title("MaxxBass original path, Harmonics -inf")
    fig("linear_paths.png")
    RES["linear"] = out
    for k, v in out.items(): print(k, v)


# ---------------------------------------------------------------- input gain
def sec_input():
    out = {}
    x = tone(1000, 0.5, -30)
    ref = mb(x)[0]
    for ig in (-12.0, -6.0, 6.0, 12.0):
        y = mb(x, input_gain=ig)[0]
        out[f"{ig:g}"] = round(R.rms_db(y[FS // 4:]) - R.rms_db(ref[FS // 4:]), 3)
    RES["input_gain_1k"] = out
    print(out)


# ---------------------------------------------------------------- superposition: Audio = orig path + MaxxBass monitor?
def sec_superpos():
    out = {}
    rng = np.random.default_rng(1)
    t = np.arange(2 * FS) / FS
    sig = (0.3 * np.sin(2 * np.pi * 55 * t) * (1 + 0.5 * np.sin(2 * np.pi * 1.3 * t)) + 0.05 * rng.standard_normal(len(t))).astype(np.float32)
    origmon = mb(sig, harmonics_gain=-10.0, orig_bass_gain=-6.0, monitor=2)[0]
    origmon0 = mb(sig, monitor=2)[0]
    out["origmon_independent_of_faders_db"] = round(R.rms_db(origmon - origmon0) - R.rms_db(origmon0), 1)
    # Harmonics is a plain gain on the generated part in the Audio mix; the MaxxBass monitor plays it pre-fader
    g0 = mb(sig, harmonics_gain=0.0, monitor=1)[0]
    orig = mb(sig)[0]
    for hg in (-24.0, -10.0, 6.0, 12.0):
        a = mb(sig, harmonics_gain=hg)[0]
        g = mb(sig, harmonics_gain=hg, monitor=1)[0]
        out[f"audio_minus_orig_minus_gain_x_gen0_{hg:g}_db"] = round(R.rms_db(a - orig - 10 ** (hg / 20) * g0) - R.rms_db(a), 1)
        out[f"monitor_gen_{hg:g}_vs_gen0_db"] = round(R.rms_db(g - g0) - R.rms_db(g0), 1)
    # does Original Bass gain change the generated part?
    for og in (NINF, -12.0, 12.0):
        g = mb(sig, harmonics_gain=0.0, orig_bass_gain=og, monitor=1)[0]
        out[f"gen_vs_orig{og:g}_db"] = round(R.rms_db(g - g0) - R.rms_db(g0), 1)
    # input gain before the generator?
    for ig in (-6.0, 6.0):
        g = mb(sig, harmonics_gain=0.0, input_gain=ig, monitor=1)[0]
        out[f"gen_input{ig:g}_level_change_db"] = round(R.rms_db(g) - R.rms_db(g0), 2)
    RES["superposition"] = out
    for k, v in out.items(): print(k, v)


# ---------------------------------------------------------------- harmonic content of the generated part
def gen_tone(f0, dbfs, secs=1.5, **kw):
    y = mb(tone(f0, secs, dbfs), harmonics_gain=0.0, monitor=1, **kw)[0]
    return harmonic_amps(y, f0) / 10 ** (dbfs / 20)


def sec_harmonics():
    out = {}
    rows = []
    for F in (32, 80, 256):
        for dec in (-24.0, -15.0, -9.0):
            for f0 in (16, 20, 25, 30, 40, 50, 60, 70, 80, 100, 120, 160, 200, 256, 320):
                if f0 > 2.2 * F: continue
                a = gen_tone(f0, -20, frequency=F, harmonics_decay=dec)
                rows.append([F, dec, f0] + list(np.round(db(a), 2)))
                out[f"F{F}_dec{dec:g}_f{f0}"] = [round(float(v), 1) for v in db(a)]
    csv("harmonics.csv", "F,decay,f0," + ",".join(f"h{h}_db" for h in range(1, 9)), rows)
    # decay: the step between successive harmonics?
    RES["harmonics"] = out
    for F in (80,):
        plt.figure(figsize=(9, 5))
        for dec in (-24.0, -15.0, -9.0):
            for f0 in (40, 60):
                v = out.get(f"F{F}_dec{dec:g}_f{f0}")
                if v: plt.plot(range(1, 9), v, "o-", label=f"decay {dec:g}, f0 {f0}")
        plt.grid(alpha=0.3); plt.legend(); plt.xlabel("harmonic"); plt.ylabel("dB re input"); plt.title("Generated part, F 80, -20 dBFS")
        fig("harmonics_decay.png")
    print("harmonics:", len(rows), "rows")
    for k in ("F80_dec-24_f40", "F80_dec-15_f40", "F80_dec-9_f40", "F80_dec-15_f60", "F80_dec-15_f100", "F32_dec-15_f20", "F256_dec-15_f100"):
        print(k, out.get(k))


# ---------------------------------------------------------------- level dependence and Ratio
def sec_level():
    out = {}
    rows = []
    for ratio in (1.0, 1.41, 2.0, 4.0):
        for lv in (-60, -48, -36, -24, -18, -12, -6, 0):
            a = gen_tone(50, lv, ratio=ratio)
            rows.append([ratio, lv] + list(np.round(db(a[:4]), 2)))
            out[f"r{ratio:g}_{lv}"] = [round(float(v), 1) for v in db(a[:4])]
    csv("level.csv", "ratio,level_dbfs,h1,h2,h3,h4", rows)
    RES["level"] = out
    plt.figure(figsize=(8, 5))
    for ratio in (1.0, 1.41, 2.0, 4.0):
        lv = [-60, -48, -36, -24, -18, -12, -6, 0]
        tot = [db(np.sqrt(np.sum(10 ** (np.array(out[f"r{ratio:g}_{l}"]) / 10)))) + l for l in lv]
        plt.plot(lv, tot, "o-", label=f"ratio {ratio:g}")
    plt.plot([-60, 0], [-60, 0], "k:", lw=0.8)
    plt.grid(alpha=0.3); plt.legend(); plt.xlabel("input dBFS (50 Hz)"); plt.ylabel("generated h1..h4 total, dBFS"); plt.title("Generated level vs input")
    fig("level_ratio.png")
    for k, v in out.items(): print(k, v)


# ---------------------------------------------------------------- dynamics: level steps
def env(y, f0):
    """Amplitude envelope via the analytic signal of the generated part, low-passed below f0."""
    a = np.abs(ss.hilbert(y))
    b, aa = ss.butter(2, f0 / 2, fs=FS)
    return ss.filtfilt(b, aa, a)


def sec_dynamics():
    out = {}
    f0 = 50
    t = np.arange(int(2.4 * FS)) / FS
    step = np.where((t >= 0.8) & (t < 1.6), 10 ** (-10 / 20), 10 ** (-40 / 20))
    x = (step * np.sin(2 * np.pi * f0 * t)).astype(np.float32)
    plt.figure(figsize=(10, 6))
    for ratio in (1.0, 4.0):
        for resp in (10.0, 20.0, 30.0):
            y = mb(x, harmonics_gain=0.0, monitor=1, ratio=ratio, response=resp)[0]
            e = db(env(y, f0))
            i_up, i_dn = int(0.8 * FS), int(1.6 * FS)
            fin_up = np.median(e[i_up + int(0.5 * FS): i_dn - 1000]); fin_dn = np.median(e[i_dn + int(0.6 * FS):])
            pre = np.median(e[i_up - int(0.3 * FS): i_up - 1000])
            def tsettle(seg, final, start):
                d = np.abs(seg - final)
                bad = np.where(d > 1.0)[0]
                return (bad[-1] + 1) / FS * 1000 if len(bad) else 0.0
            out[f"r{ratio:g}_resp{resp:g}"] = {
                "low_db": round(float(pre), 2), "high_db": round(float(fin_up), 2), "after_db": round(float(fin_dn), 2),
                "overshoot_up_db": round(float(np.max(e[i_up:i_up + int(0.3 * FS)]) - fin_up), 2),
                "undershoot_dn_db": round(float(np.min(e[i_dn:i_dn + int(0.4 * FS)]) - fin_dn), 2),
                "settle_up_ms_1dB": round(tsettle(e[i_up:i_up + int(0.5 * FS)], fin_up, i_up), 1),
                "settle_dn_ms_1dB": round(tsettle(e[i_dn:i_dn + int(0.6 * FS)], fin_dn, i_dn), 1)}
            plt.plot(t[::48], e[::48], lw=0.8, label=f"ratio {ratio:g}, response {resp:g}")
    plt.xlim(0.6, 2.2); plt.ylim(-80, 0); plt.grid(alpha=0.3); plt.legend(fontsize=8)
    plt.xlabel("s"); plt.ylabel("generated envelope dBFS"); plt.title("50 Hz tone, -40 -> -10 -> -40 dBFS")
    fig("dynamics_steps.png")
    RES["dynamics"] = out
    for k, v in out.items(): print(k, v)


# ---------------------------------------------------------------- Response with ratio 1 (does it do anything?)
def sec_response():
    out = {}
    rng = np.random.default_rng(3)
    t = np.arange(2 * FS) / FS
    sig = (0.3 * np.sin(2 * np.pi * 45 * t) * (1 + 0.7 * np.sin(2 * np.pi * 3 * t)) + 0.02 * rng.standard_normal(len(t))).astype(np.float32)
    for ratio in (1.0, 2.0, 4.0):
        ref = mb(sig, harmonics_gain=0.0, monitor=1, ratio=ratio, response=20.0)[0]
        for resp in (10.0, 30.0):
            y = mb(sig, harmonics_gain=0.0, monitor=1, ratio=ratio, response=resp)[0]
            out[f"r{ratio:g}_resp{resp:g}_vs20_db"] = round(R.rms_db(y - ref) - R.rms_db(ref), 1)
    RES["response"] = out
    print(out)


# ---------------------------------------------------------------- high-pass on the generated part
def sec_highpass():
    out = {}
    for F in (40, 80, 160):
        for hp in (0, 1, 2):
            v = {}
            for f0 in (15, 20, 25, 30, 40, 50, 60, 80, 100, 120):
                a = gen_tone(f0, -20, frequency=F, highpass=hp)
                v[str(f0)] = [round(float(x), 1) for x in db(a[:4])]
            out[f"F{F}_hp{hp}"] = v
    RES["highpass"] = out
    for k in ("F80_hp0", "F80_hp1", "F80_hp2"):
        print(k, {f: out[k][f][:3] for f in ("20", "40", "80", "120")})


# ---------------------------------------------------------------- stereo
def sec_stereo():
    out = {}
    x = tone(50, 1.5, -12)
    z = np.zeros_like(x)
    yl = mb(np.stack([x, z]), harmonics_gain=0.0, monitor=1)
    yb = mb(np.stack([x, x]), harmonics_gain=0.0, monitor=1)
    out["left_only_genL_db"] = round(R.rms_db(yl[0][FS // 2:]), 2)
    out["left_only_genR_db"] = round(R.rms_db(yl[1][FS // 2:]), 2)
    out["both_genL_db"] = round(R.rms_db(yb[0][FS // 2:]), 2)
    q = tone(50, 1.5, -40)
    yq = mb(np.stack([x, q]), harmonics_gain=0.0, monitor=1)
    yq0 = mb(np.stack([z, q]), harmonics_gain=0.0, monitor=1)
    out["quiet_right_gen_with_loud_left_db"] = round(R.rms_db(yq[1][FS // 2:]), 2)
    out["quiet_right_gen_alone_db"] = round(R.rms_db(yq0[1][FS // 2:]), 2)
    a = harmonic_amps(yq[1], 50); b = harmonic_amps(yq0[1], 50)
    out["quiet_right_h2_with_vs_alone_db"] = [round(float(db(a[1])), 1), round(float(db(b[1])), 1)]
    # mono component
    ym = mb(x, name="MaxxBass Mono", harmonics_gain=0.0, monitor=1)
    out["mono_vs_stereo_both_db"] = round(R.rms_db(ym[0] - yb[0]) - R.rms_db(yb[0]), 1)
    RES["stereo"] = out
    print(out)


# ---------------------------------------------------------------- clipping
def sec_clip():
    out = {}
    for lv in (0.0, 6.0):
        for hg in (NINF, 12.0):
            x = tone(50, 1.0, lv) if lv <= 0 else (tone(50, 1.0, 0.0) * 10 ** (lv / 20)).astype(np.float32)
            y = mb(x, harmonics_gain=hg)
            out[f"in{lv:g}_harm{hg:g}_peak"] = round(float(np.max(np.abs(y))), 6)
    x = (tone(1000, 0.5, 0.0) * 2).astype(np.float32)
    y = mb(x)[0]
    out["1k_+6dBFS_peak"] = repr(float(np.max(np.abs(y))))
    out["1k_+6dBFS_min"] = repr(float(np.min(y)))
    out["1k_+6dBFS_fraction_at_peak"] = round(float(np.mean(np.abs(y) >= np.max(np.abs(y)) - 1e-7)), 3)
    lin = mb((x * 0.1).astype(np.float32))[0] * 10.0
    out["1k_+6dBFS_vs_hard_clip_db"] = round(R.rms_db(y - np.clip(lin, -1.0, 1.0)) - R.rms_db(y), 1)
    x = (tone(1000, 0.5, 0.0) * 2).astype(np.float32)
    out["1k_+6dBFS_monitor_gen_peak"] = round(float(np.max(np.abs(mb(x, harmonics_gain=12.0, monitor=1)))), 6)
    y = mb(tone(40, 1.0, -3), harmonics_gain=12.0, monitor=1)
    out["gen_+12_40Hz_-3dBFS_peak"] = round(float(np.max(np.abs(y))), 6)
    RES["clip"] = out
    print(out)


# ---------------------------------------------------------------- smoothing of parameter changes
def sec_smoothing():
    """A parameter change between two consecutive blocks (no reset). The output is written as y = ya + w (yb - ya)
    from steady renders at the old and new values; w(t) is solved over 1 ms windows."""
    out = {}
    p = R.load("MaxxBass Stereo")
    cases = (("orig_bass_gain", 0.0, -12.0, 40, {}), ("input_gain", 0.0, -12.0, 1000, {}),
             ("harmonics_gain", -12.0, 0.0, 30, {"orig_bass_gain": NINF}), ("frequency", 80.0, 160.0, 100, {}))
    for par, a, b, f0, extra in cases:
        sig = np.stack([tone(f0, 1.0, -20)] * 2)
        h = FS // 2
        R.set_mb(p, **{par: a}, **extra)
        ya = p(sig, FS, buffer_size=512, reset=True)[0]
        R.set_mb(p, **{par: b}, **extra)
        yb = p(sig, FS, buffer_size=512, reset=True)[0]
        R.set_mb(p, **{par: a}, **extra)
        y1 = p(sig[:, :h], FS, buffer_size=512, reset=True)
        setattr(p, par, float(b))
        y2 = p(sig[:, h:], FS, buffer_size=512, reset=False)
        y = np.concatenate([y1, y2], axis=1)[0]
        n = 48
        w = []
        for i in range(h - 10 * n, h + 60 * n, n):
            d = yb[i:i + n] - ya[i:i + n]
            w.append(float(np.dot(y[i:i + n] - ya[i:i + n], d) / max(np.dot(d, d), 1e-20)))
        w = np.array(w); ms = np.arange(-10, 60)
        def first(th):
            i = np.where((ms >= 0) & (w >= th))[0]
            return int(ms[i[0]]) if len(i) else None
        out[par] = {"w_before": round(float(np.median(w[:9])), 3), "w_end": round(float(np.median(w[-10:])), 3),
                    "t50_ms": first(0.5), "t90_ms": first(0.9), "w_per_ms": [round(float(v), 3) for v in w[10:40]]}
    R.set_mb(p)
    RES["smoothing"] = out
    for k, v in out.items(): print(k, v)


# ---------------------------------------------------------------- MaxxBass vs RBass
def sec_rbass():
    """Same tones through RBass (Original Bass In, Intensity 0) and MaxxBass (Harmonics 0): how alike are the
    generated parts? RBass's generated part is split off with an Intensity -inf-like render (-24 then scaled)."""
    out = {}
    for f0 in (40, 60, 100):
        for lv in (-30, -12):
            x = tone(f0, 1.5, lv)
            m = mb(x, harmonics_gain=0.0, monitor=1)[0]
            r0 = R.render_rbass(x, FS, freq=80, intensity=0.0, hp=1)[0]
            ro = R.render_rbass(x, FS, freq=80, intensity=-24.0, hp=1)[0]      # Intensity is an exact gain on RBass's generated part
            am = db(harmonic_amps(m, f0)[:4]) - lv; ar = db(harmonic_amps((r0 - ro) / (1 - 10 ** (-24 / 20)), f0)[:4]) - lv
            out[f"f{f0}_{lv}"] = {"maxxbass": [round(float(v), 1) for v in am], "rbass": [round(float(v), 1) for v in ar]}
    RES["vs_rbass"] = out
    for k, v in out.items(): print(k, v)


SECTIONS = dict(map=sec_map, latency=sec_latency, linear=sec_linear, input=sec_input, superpos=sec_superpos,
                harmonics=sec_harmonics, level=sec_level, dynamics=sec_dynamics, response=sec_response,
                highpass=sec_highpass, stereo=sec_stereo, clip=sec_clip, smoothing=sec_smoothing, rbass=sec_rbass)

if __name__ == "__main__":
    for s in (sys.argv[1:] or list(SECTIONS)):
        print("==", s)
        SECTIONS[s]()
        save()
    print("retries:", R.STATS)
