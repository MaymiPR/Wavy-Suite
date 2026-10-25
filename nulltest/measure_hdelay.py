"""WDelay Phase 3: measurements of the reference H-Delay (WaveShell VST3) through tools/RefRender (nulltest/hdelay.py).

    python -I measure_hdelay.py            writes measurements/hdelay/results.json and the kernels (.csv)

Each probe prints what it finds; MEASUREMENTS.md "WDelay (H-Delay) measurements" summarises them. Settings are made
after a silent pre-roll (H-Delay ignores values set before its first block); a delay time set together with Sync is
not taken, so times are set as changes inside the pre-roll, after Sync.
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import optimize, signal, special
from scipy.signal import hilbert
import hdelay as H

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hdelay')
FS = 48000
R = {}


def ms(delay, at=-0.5):                                   # a time in MS sync, set inside the pre-roll
    return dict(sync="ms", changes=[(at, "delay", delay)])


def impulse(n, i0, a, fs=FS):
    x = np.zeros((2, n), np.float32); x[:, i0] = a; return x


def kaiser_sinc(K, frac, fc, Lw, beta):
    x = K - frac
    w = np.where(np.abs(x) < Lw, special.i0(beta * np.sqrt(np.clip(1 - (x / Lw) ** 2, 0, 1))) / special.i0(beta), 0.0)
    return 2 * fc * np.sinc(2 * fc * x) * w


def fit_kernel(m, K, p0s):
    best = None
    for p0 in p0s:
        f = lambda p: ((kaiser_sinc(K, p[3], p[0], p[1], p[2]) - m) ** 2).sum()
        r = optimize.minimize(f, p0, method='Nelder-Mead', options={'xatol': 1e-8, 'fatol': 1e-15, 'maxiter': 8000})
        if best is None or r.fun < best.fun: best = r
    fc, Lw, beta, frac = best.x
    return dict(cutoff_norm=float(fc), half_length=float(Lw), beta=float(abs(beta)), frac=float(frac),
                max_err=float(np.abs(kaiser_sinc(K, frac, fc, Lw, beta) - m).max()))


def kernels():
    """the echo of a tiny impulse: the interpolator at a whole-sample delay (48 / 44.1 / 96 kHz, LoFi)"""
    K = np.arange(-60, 61)
    for name, fs, kw in (("48k", 48000, {}), ("44k1", 44100, {}), ("96k", 96000, {}), ("48k_lofi", 48000, dict(lofi="LoFi"))):
        d = int(round(0.375 * fs)); i0 = 1000
        y = H.render(impulse(fs, i0, 0.001), fs, preroll=2, feedback=0, analog="Off", **kw)[0] / 0.001 * 1.001
        m = y[i0 + d - 60:i0 + d + 61]
        np.savetxt(os.path.join(OUT, 'kernel_%s.csv' % name), np.c_[K, m], fmt='%d,%.8f', header='tap,value', comments='')
        fc0 = {"96k": 0.25, "48k_lofi": 6280 / 48000}.get(name, 0.5)
        L0 = 31.6 if name == "96k" else 15.8
        R['kernel_' + name] = fit_kernel(m, K, [[fc0, L0, 4.88, f] for f in (1 / 32, -30 / 32, 0.0)])
        R['kernel_' + name]['sum'] = float(m.sum())
        print('kernel %s: %s' % (name, R['kernel_' + name]))


def interpolation_phases():
    """impulses every 5 ms during a slow glide (375 -> 374 ms): each echo's fraction, fitted with the 48 kHz kernel"""
    k = R['kernel_48k']; n = int(FS * 4.0); per = 240
    x = np.zeros(n, np.float32); pos = np.arange(2000, n - 20000, per); x[pos] = 0.001
    y = H.render(np.stack([x, x]), FS, preroll=2, sync="ms", changes=[(-1.5, "delay", 375), (0.0, "delay", 374)],
                 feedback=0, analog="Off")[0] / 0.001 * 1.001
    T = 374 * 48.0; d = 18000.0; D = np.zeros(n)
    for i in range(n): D[i] = d; d += max(-1.0, (T - d) / T)
    K = np.arange(-24, 25); fr, meas, model = [], [], []
    for p in pos:
        t = int(round(p + D[min(n - 1, p + 18000)]))
        if t + 25 >= n: break
        s = y[t - 24:t + 25]; s = s / s.sum()
        r = optimize.minimize_scalar(lambda f: ((kaiser_sinc(K, f, 0.5, k['half_length'], k['beta']) - s) ** 2).sum(),
                                     bounds=(-0.6, 0.6), method='bounded', options={'xatol': 1e-6})
        if r.fun ** 0.5 / 7 > 5e-4: continue                   # the delay moved within the kernel
        fr.append(r.x * 32); meas.append(t - p + r.x); model.append((np.floor(D[t] * 32) + 1) / 32)
    fr = np.array(fr); e = np.array(meas) - np.array(model)
    R['interpolation'] = dict(phases_per_sample=32, fraction_x32_off_integer_median=float(np.median(np.abs(fr - np.round(fr)))),
                              delay_vs_ceil32_model_rms=float(np.sqrt((e ** 2).mean())), max=float(np.abs(e).max()), echoes=len(fr))
    print('interpolation:', R['interpolation'])


def gain_law():
    """the wet gain against the level of an impulse, and its release after a large one (impulse pairs)"""
    i0 = 4800; out = []
    for a in (1.0, 0.5, 0.25, 0.1, 0.01):
        y = H.render(impulse(FS, i0, a), FS, preroll=2, feedback=0, analog="Off")[0] / a
        out.append((a, float(y[i0 + 18000 - 30:i0 + 18000 + 31].sum())))
    R['gain_vs_impulse'] = out
    print('wet gain vs impulse level (1/(1+a)):', out)
    gaps = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096]; period = int(1.0 * FS)
    n = period * len(gaps) + FS
    big = np.zeros(n, np.float32); both = np.zeros(n, np.float32)
    for k, g in enumerate(gaps):
        t0 = k * period + 1000; big[t0] = both[t0] = 1.0; both[t0 + g] = 0.01
    yb = H.render(np.stack([big, big]), FS, preroll=2, feedback=0, analog="Off")[0]
    ya = H.render(np.stack([both, both]), FS, preroll=2, feedback=0, analog="Off")[0]
    rel = []
    for k, g in enumerate(gaps):
        t0 = k * period + 1000 + g + 18000
        rel.append((g, float((ya - yb)[t0 - 30:t0 + 31].sum() / 0.01)))
    R['gain_release'] = rel
    # the gain recovers toward 1 as a one-pole on the gain: 1 - g = 0.5 r^gap (above the small impulse's own 1/1.01)
    g_ = np.array(rel); m = (g_[:, 0] >= 16) & (g_[:, 0] <= 1024)
    r = np.exp(np.polyfit(g_[m, 0], np.log(np.clip(1 / 1.01 - g_[m, 1], 1e-9, None) / 0.5), 1)[0])
    R['gain_release_coefficient'] = float(r); R['gain_release_ms'] = float(-1 / np.log(r) / FS * 1000)
    print('release: %s; per-sample coefficient %.6f (%.2f ms)' % (rel, r, R['gain_release_ms']))
    x = np.zeros((2, FS), np.float32); x[0, i0] = 1.0; x[1, i0] = 0.01
    y = H.render(x, FS, preroll=2, feedback=0, analog="Off")
    R['gain_stereo_linked_R'] = float(y[1, i0 + 18000 - 30:i0 + 18000 + 31].sum() / 0.01)
    print('linked: R gain with a big L impulse %.4f' % R['gain_stereo_linked_R'])


def feedback():
    D = 4800; i0 = 1000; res = {}
    for fb in (10, 30, 60, 100, 150, 200):
        y = H.render(impulse(int(FS * 1.6), i0, 0.001), FS, preroll=2, feedback=fb, analog="Off", **ms(100, -1.5))
        e = [float(y[0, i0 + k * D - 30:i0 + k * D + 31].sum() / 0.001) for k in range(1, 10)]
        pk = [int(np.argmax(np.abs(y[0, i0 + k * D - 30:i0 + k * D + 31])) - 30) for k in range(1, 6)]
        res[fb] = dict(echoes=e, peak_offsets=pk)
        print('feedback %d: echoes %s, peak offsets %s' % (fb, np.round(e[:6], 4), pk))
    R['feedback'] = res


def glide():
    """the delay after a change, from the phase of a 500 Hz tone"""
    n = FS * 9; t = np.arange(n) / FS; f = 500.0
    x = (0.3 * np.sin(2 * np.pi * f * t)).astype(np.float32)
    res = {}
    for target in (100, 50, 1000, 3500, 365):
        y = H.render(np.stack([x, x]), FS, sync="ms", feedback=0, analog="Off", changes=[(-0.9, "delay", 375), (2.0, "delay", target)])
        ph = np.unwrap(np.angle(hilbert(y[0]))); d = (2 * np.pi * f * t - np.pi / 2 - ph) / (2 * np.pi * f) * 1000
        d += 375 - d[int(1.9 * FS)]
        # the law: d' = clamp((T - d) / T, -1, 1)
        T = target; m = 375.0; sim = np.zeros(n)
        for i in range(n):
            sim[i] = m
            if i >= 2 * FS: m += max(-1.0, (T - m) / T) / FS * 1000
        seg = slice(int(1.95 * FS), n - 1000)
        res[target] = dict(t=[round(v, 3) for v in t[seg][::480].tolist()], measured_ms=np.round(d[seg][::480], 3).tolist(),
                           model_rms_ms=float(np.sqrt(((d[seg] - sim[seg]) ** 2).mean())))
        print('glide 375 -> %d: model rms %.3f ms' % (target, res[target]['model_rms_ms']))
    R['glide'] = res


def filters_and_lofi():
    freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 20000]; D = 4800; i0 = 1000; res = {}
    for kind, val in (('hipass', 100), ('hipass', 1000), ('lopass', 1000), ('lopass', 5000), ('lofi', 'LoFi')):
        y = H.render(impulse(int(FS * 0.5), i0, 0.001), FS, preroll=2, feedback=50, analog="Off", **{kind: val}, **ms(100, -1.5)) / 0.001
        e1 = y[0, i0 + D - 64:i0 + 2 * D - 64]; e2 = y[0, i0 + 2 * D - 63:i0 + 3 * D - 64]
        fr = np.fft.rfftfreq(1 << 15, 1 / FS); idx = [np.argmin(abs(fr - f)) for f in freqs]
        m1 = 20 * np.log10(np.abs(np.fft.rfft(e1, 1 << 15)[idx])); m2 = 20 * np.log10(np.abs(np.fft.rfft(e2, 1 << 15)[idx]) / 0.5)
        res['%s_%s' % (kind, val)] = dict(f=freqs, echo1_db=np.round(m1, 3).tolist(), echo2_over_fb_db=np.round(m2, 3).tolist())
        print('%s %s: echo 1 %s' % (kind, val, np.round(m1, 2)))
    R['filters'] = res


def mix_output_routing():
    i0 = 1000; D = 18000
    ech = lambda y, c, k=1, extra=0: float(y[c, i0 + k * D + extra - 30:i0 + k * D + extra + 31].sum())
    mix = []
    for m in (0, 25, 50, 75, 100):
        y = H.render(impulse(FS, i0, 0.001), FS, preroll=2, feedback=0, analog="Off", mix=m) / 0.001
        mix.append((m, float(y[0, i0]), ech(y, 0)))
    R['mix'] = mix; print('mix (dry, wet):', mix)
    out = []
    for o in (-18, -6, 6, 18):
        y = H.render(impulse(FS, i0, 0.001), FS, preroll=2, feedback=0, analog="Off", mix=50, output=o) / 0.001
        out.append((o, float(20 * np.log10(y[0, i0] / np.sqrt(0.5)))))
    R['output_db'] = out; print('output:', out)
    pp = {}
    for start in ("Left", "Right"):
        for ch in (0, 1):
            x = np.zeros((2, FS * 2), np.float32); x[ch, i0] = 0.001
            y = H.render(x, FS, preroll=2, feedback=60, analog="Off", pingpong="PingPong", start=start) / 0.001
            pp['%s_in%s' % (start, 'LR'[ch])] = [(ech(y, 0, k, k - 1), ech(y, 1, k, k - 1)) for k in range(1, 6)]
    R['pingpong'] = pp; print('ping pong:', pp)
    y = H.render(impulse(FS * 2, i0, 0.001), FS, preroll=2, feedback=60, analog="Off", phase_l="-", mix=50) / 0.001
    R['phase_l'] = dict(dry=(float(y[0, i0]), float(y[1, i0])), echoes_L=[ech(y, 0, k, k - 1) for k in (1, 2, 3)],
                        echoes_R=[ech(y, 1, k, k - 1) for k in (1, 2, 3)])
    print('phase L:', R['phase_l'])
    y = H.render(impulse(FS * 2, i0, 0.001), FS, preroll=4, sync="ms", changes=[(-3.5, "delay", 100), (-3.5, "delay_r", 200)],
                 feedback=0, analog="Off")
    R['delay_r_without_dual_ms'] = [float((np.argmax(np.abs(y[c, i0 + 10:])) + 10) / FS * 1000) for c in (0, 1)]
    print('Delay R set last, Dual off: echoes at', R['delay_r_without_dual_ms'])


def sync():
    i0 = 1000; res = []
    for s, bpm_p, host, note, beats in (("Host", None, 120, "1/4", 1), ("Host", None, 90, "1/4T", 2 / 3), ("BPM", 100, 120, "1/8", 0.5),
                                        ("BPM", 100, 120, "1/16D", 0.375)):
        ch = [(-3.5, "note", note)] + ([(-3.5, "bpm", bpm_p)] if bpm_p else [])
        y = H.render(impulse(FS * 2, i0, 0.001), FS, preroll=4, host_bpm=host, sync=s, changes=ch, feedback=0, analog="Off")
        got = float((np.argmax(np.abs(y[0, i0 + 10:])) + 10) / FS * 1000)
        res.append(dict(sync=s, note=note, host_bpm=host, bpm=bpm_p, echo_ms=got, expected_ms=beats * 60000 / (bpm_p or host)))
    R['sync'] = res; print('sync:', res)


def modulation():
    n = FS * 6; t = np.arange(n) / FS; f = 500.0
    x = (0.01 * np.sin(2 * np.pi * f * t)).astype(np.float32); res = {}
    for depth, rate in ((100, 1), (50, 1), (25, 1), (100, 0.25), (100, 4)):
        y = H.render(np.stack([x, x]), FS, preroll=3, feedback=0, analog="Off", depth=depth, rate=rate, **ms(100, -2.5))
        ds = []
        for c in (0, 1):
            ph = np.unwrap(np.angle(hilbert(y[c]))); ds.append((2 * np.pi * f * t - np.pi / 2 - ph) / (2 * np.pi * f) * 1000)
        seg = slice(FS, 5 * FS)
        res['%d_%g' % (depth, rate)] = dict(pp_ms=float(np.ptp(ds[0][seg])), left_right_max_diff_ms=float(np.abs(ds[0][seg] - ds[1][seg]).max()))
        print('modulation depth %d rate %g: %s' % (depth, rate, res['%d_%g' % (depth, rate)]))
    R['modulation'] = res


def analog():
    n = FS * 4; rng = np.random.default_rng(2)
    x = (0.03 * rng.standard_normal(n)).astype(np.float32)
    freqs = [30, 60, 100, 200, 500, 1000, 2000, 4000, 8000, 12000, 16000, 20000]; res = {}
    for mode in ("Off", "Mod1", "Mod2", "Mod3", "Mod4"):
        y = H.render(np.stack([x, x]), FS, preroll=1, analog=mode, mix=0)[0][FS // 2:]
        z = H.render(np.zeros((2, n), np.float32), FS, preroll=1, analog=mode)[0][FS // 2:]
        f, Pxy = signal.csd(x[FS // 2:], y, FS, nperseg=8192); _, Pxx = signal.welch(x[FS // 2:], FS, nperseg=8192)
        Hf = Pxy / Pxx; idx = [np.argmin(abs(f - q)) for q in freqs]
        res[mode] = dict(f=freqs, mag_db=np.round(20 * np.log10(np.abs(Hf[idx])), 3).tolist(),
                         phase_deg=np.round(np.degrees(np.angle(Hf[idx])), 2).tolist(),
                         noise_dbfs=float(20 * np.log10(np.sqrt((z ** 2).mean()) + 1e-12)))
        print('analog %s: noise %.1f dBFS, response %s' % (mode, res[mode]['noise_dbfs'], res[mode]['mag_db']))
    R['analog'] = res


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for step in (kernels, interpolation_phases, gain_law, feedback, glide, filters_and_lofi, mix_output_routing, sync, modulation, analog):
        step()
        json.dump(R, open(os.path.join(OUT, 'results.json'), 'w'), indent=1)
    print('wrote', os.path.join(OUT, 'results.json'))
