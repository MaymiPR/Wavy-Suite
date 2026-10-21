"""Phase 3 measurements of the reference H-Comp (private analysis; the rendered audio is not published).

    python -I measure_hcomp.py [section ...]

Sections: linear static detector freqdet timing punch release_pd makeup knee mix output analog stereo rates sync plots
(default: all). Results go to <repo>/measurements/hcomp/: results.json, csv tables and png plots.
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import scipy.signal as ss
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import refhost as R

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "hcomp")
os.makedirs(OUT, exist_ok=True)
FS = 48000
RES_PATH = os.path.join(OUT, "results.json")
RES = json.load(open(RES_PATH)) if os.path.exists(RES_PATH) else {}


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


def hc(x, **kw):
    return R.render_hc(x, FS, **kw)


def sine(f, secs, amp, phase=0.0):
    t = np.arange(int(secs * FS)) / FS
    return (amp * np.sin(2 * np.pi * f * t + phase)).astype(np.float32)


def amp_at(y, f, a, b):
    """Amplitude of the f component of y over samples a..b (whole periods)."""
    per = FS / f
    n = int(int((b - a) / per) * per)
    seg = np.asarray(y[a:a + n], np.float64)
    t = np.arange(a, a + n) / FS
    return 2 * abs(np.mean(seg * np.exp(-2j * np.pi * f * t)))


def staircase(levels_db, step_s, f=1000.0):
    """Sine bursts at the given peak levels, each step_s long, phase continuous."""
    n = int(step_s * FS)
    t = np.arange(n * len(levels_db)) / FS
    env = np.repeat(10 ** (np.asarray(levels_db) / 20), n)
    return (env * np.sin(2 * np.pi * f * t)).astype(np.float32), n


def step_gains(y, levels_db, n, f=1000.0, tail=0.25):
    """Steady gain (dB) at the end of each step of a staircase."""
    out = []
    for i, L in enumerate(levels_db):
        b = (i + 1) * n
        a = b - int(tail * FS)
        out.append(db(amp_at(y, f, a, b)) - L)
    return np.array(out)


# ----------------------------------------------------------------------------------------------------------------
def sec_linear():
    """Idle path (threshold 0, ratio 1): identity at Analog Off? Frequency response of each Analog mode."""
    x = np.zeros(FS, np.float32); x[2000] = 0.05
    r = {}
    for an in ('Off', 1, 2, 3, 4):
        y = hc(x, threshold=0.0, ratio=1.0, analog=an)[0]
        h = y[2000:2000 + 16384] / 0.05
        H = np.fft.rfft(h, 65536)
        fr = np.fft.rfftfreq(65536, 1 / FS)
        sel = [50, 100, 1000, 5000, 10000, 15000, 20000]
        r[str(an)] = {'impulse_peak': float(h.max()), 'peak_index': int(np.argmax(np.abs(h))),
                      'resp_db': {str(f): float(db(H[np.argmin(np.abs(fr - f))])) for f in sel}}
        print('analog', an, r[str(an)])
    RES['linear'] = r
    save()


def sec_static():
    """Static curves: steady gain against peak level of a 1 kHz sine, for thresholds x ratios (up and down)."""
    levels = list(range(-66, 1, 2))
    rows = []
    out = {}
    for thr in (0.0, -6.0, -12.0, -18.0, -24.0, -36.0, -48.0):
        for rat in (1.5, 2.0, 3.0, 4.0, 8.0, 20.0, 50.0):
            x, n = staircase(levels, 0.6)
            y = hc(x, threshold=thr, ratio=rat, attack=7.0, release=100.0, punch=0.0)[0]
            g = step_gains(y, levels, n)
            xd, _ = staircase(levels[::-1], 0.6)
            yd = hc(xd, threshold=thr, ratio=rat, attack=7.0, release=100.0, punch=0.0)[0]
            gd = step_gains(yd, levels[::-1], n)[::-1]
            out['%g/%g' % (thr, rat)] = {'up': g.tolist(), 'down': gd.tolist()}
            for L, a, b in zip(levels, g, gd): rows.append((thr, rat, L, a, b))
            print('thr %5.1f ratio %5.1f  makeup %+6.2f  gain at 0 dB %+6.2f  up/down max diff %.3f' % (thr, rat, g[0], g[-1], np.abs(g - gd).max()), flush=True)
    RES['static'] = {'levels': levels, 'curves': out}
    csv('static.csv', 'threshold,ratio,level_peak_db,gain_up_db,gain_down_db', rows)
    save()
    plt.figure(figsize=(8, 6))
    for thr in (0.0, -18.0, -36.0):
        for rat in (2.0, 4.0, 50.0):
            g = np.array(out['%g/%g' % (thr, rat)]['up'])
            plt.plot(levels, np.array(levels) + g, label='%g dB, %g:1' % (thr, rat))
    plt.xlabel('input peak (dB)'); plt.ylabel('output (dB)'); plt.grid(); plt.legend(fontsize=7)
    fig('static.png')


def sec_detector():
    """What the detector reads: steady GR for a sine, a square wave and noise at the same peak and the same RMS."""
    r = {}
    thr, rat = -24.0, 50.0
    base_gain = None
    for name, gen in (('sine', lambda a: sine(1000, 2.0, a)),
                      ('square', lambda a: (a * np.sign(np.sin(2 * np.pi * 1000 * np.arange(2 * FS) / FS + 1e-3))).astype(np.float32)),
                      ('alt', lambda a: (a * (1 - 2 * (np.arange(2 * FS) % 2))).astype(np.float32))):
        for L in (-12.0, -6.0):
            x = gen(10 ** (L / 20))
            y = hc(x, threshold=thr, ratio=rat, punch=0.0)[0]
            g = 20 * np.log10(np.sqrt(np.mean(y[-FS // 2:] ** 2)) / np.sqrt(np.mean(x[-FS // 2:] ** 2)))
            r['%s %g' % (name, L)] = float(g)
            print(name, L, 'gain %.3f' % g, flush=True)
    RES['detector'] = r
    save()


def sec_freqdet():
    """Gain reduction against frequency at a fixed peak level (side-chain filtering)."""
    r = {}
    for f in (30, 50, 80, 120, 200, 400, 1000, 3000, 8000, 15000):
        x = sine(f, 2.0, 10 ** (-6 / 20))
        y = hc(x, threshold=-24.0, ratio=4.0, punch=0.0)[0]
        g = db(amp_at(y, f, FS, 2 * FS)) - db(10 ** (-6 / 20))
        r[str(f)] = float(g)
        print('f %6d  gain %+.3f' % (f, g), flush=True)
    RES['freqdet'] = r
    save()


# ---- dynamics: the sample-wise gain is y / x for an alternating +-a signal (constant |x| for the peak detector) ----
def alt(levels_db, durs_s):
    """+-a at Nyquist, a stepping through levels (peak dB) for the given durations."""
    env = np.concatenate([np.full(int(d * FS), 10 ** (L / 20)) for L, d in zip(levels_db, durs_s)])
    return (env * (1 - 2 * (np.arange(len(env)) % 2))).astype(np.float32)


def gain_trace(x, y):
    return np.asarray(y, np.float64) / np.asarray(x, np.float64)


def sec_timing():
    """Attack and release: steps of an alternating signal across the threshold; the gain trace, its time constants."""
    r = {'attack': {}, 'release': {}}
    rows = []
    thr, rat = -30.0, 50.0
    t0 = int(0.5 * FS)
    for att in (0.5, 1.0, 3.0, 7.0, 20.0, 50.0, 100.0):
        x = alt([-50.0, -10.0], [0.5, 1.0])
        g = db(gain_trace(x, hc(x, threshold=thr, ratio=rat, attack=att, release=100.0, punch=0.0)[0]))
        g0, g1 = np.median(g[t0 - 2000:t0 - 200]), np.median(g[-2000:])
        frac = (g - g0) / (g1 - g0)                    # 0 before, 1 settled
        def cross(p):
            i = np.argmax(frac[t0 - 200:] >= p)
            return (i - 200) / FS * 1000
        r['attack'][str(att)] = {'before_db': float(g0), 'after_db': float(g1), 't10_ms': cross(0.1), 't63_ms': cross(0.632), 't90_ms': cross(0.9),
                                 'first_move_ms': float((np.argmax(np.abs(g[t0 - 400:] - g0) > 0.01) - 400) / FS * 1000)}
        np.save(os.path.join(OUT, 'attack_%g.npy' % att), g[t0 - 480:t0 + int(0.4 * FS)].astype(np.float32))
        print('attack %6.1f ms:' % att, r['attack'][str(att)], flush=True)
    for rel in (3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0):
        x = alt([-10.0, -50.0], [1.0, max(1.0, rel / 1000 * 8)])
        g = db(gain_trace(x, hc(x, threshold=thr, ratio=rat, attack=7.0, release=rel, punch=0.0)[0]))
        t1 = FS
        g0, g1 = np.median(g[t1 - 2000:t1 - 200]), np.median(g[-500:])
        frac = (g - g0) / (g1 - g0)
        def cross(p):
            i = np.argmax(frac[t1 - 200:] >= p)
            return (i - 200) / FS * 1000
        r['release'][str(rel)] = {'before_db': float(g0), 'after_db': float(g1), 't10_ms': cross(0.1), 't63_ms': cross(0.632), 't90_ms': cross(0.9)}
        np.save(os.path.join(OUT, 'release_%g.npy' % rel), g[t1 - 480:].astype(np.float32))
        print('release %6.1f ms:' % rel, r['release'][str(rel)], flush=True)
    RES['timing'] = r
    save()


def sec_punch():
    """Punch: the gain trace around an upward step for punch 0 .. 30 (attack 0.5 and 7 ms)."""
    r = {}
    t0 = int(0.5 * FS)
    for att in (0.5, 7.0):
        for pu in (0.0, 1.0, 3.0, 10.0, 20.0, 30.0):
            x = alt([-50.0, -10.0], [0.5, 0.5])
            g = db(gain_trace(x, hc(x, threshold=-30.0, ratio=50.0, attack=att, release=100.0, punch=pu)[0]))
            g0 = np.median(g[t0 - 2000:t0 - 300])
            first = (np.argmax(np.abs(g[t0 - 400:] - g0) > 0.01) - 400) / FS * 1000
            r['%g/%g' % (att, pu)] = {'first_move_ms': float(first), 'g_at_0': float(g[t0] - g0),
                                      'g_at_1ms': float(g[t0 + 48] - g0), 'g_at_5ms': float(g[t0 + 240] - g0), 'g_at_20ms': float(g[t0 + 960] - g0)}
            np.save(os.path.join(OUT, 'punch_%g_%g.npy' % (att, pu)), g[t0 - 480:t0 + int(0.2 * FS)].astype(np.float32))
            print('attack %g punch %g:' % (att, pu), r['%g/%g' % (att, pu)], flush=True)
    RES['punch'] = r
    save()


def sec_release_pd():
    """Is the release program dependent? Release after bursts of different length and depth."""
    r = {}
    for burst in (0.005, 0.02, 0.1, 0.5, 2.0):
        for lvl in (-20.0, -10.0, 0.0):
            x = alt([-60.0, lvl, -60.0], [0.3, burst, 1.5])
            g = db(gain_trace(x, hc(x, threshold=-30.0, ratio=4.0, attack=0.5, release=100.0, punch=0.0)[0]))
            t1 = int((0.3 + burst) * FS)
            g0 = g[t1 - 1]; g1 = np.median(g[-2000:])
            frac = (g[t1:] - g0) / (g1 - g0)
            t63 = np.argmax(frac >= 0.632) / FS * 1000
            r['%g/%g' % (burst, lvl)] = {'gr_at_end_db': float(g0 - g1), 't63_ms': float(t63)}
            print('burst %5.3f s level %5.1f: GR %.2f dB, release t63 %.1f ms' % (burst, lvl, g0 - g1, t63), flush=True)
    RES['release_pd'] = r
    save()


def sec_makeup():
    """Automatic makeup gain on a dense grid: the gain of a tone far below every threshold."""
    x = alt([-80.0], [0.15])
    rats = [1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 8.0, 10.0, 14.0, 20.0, 30.0, 50.0]
    rows = []
    for thr in np.arange(0.0, -48.01, -2.0):
        for rat in rats:
            y = hc(x, threshold=float(thr), ratio=rat)[0]
            m = float(np.median(db(gain_trace(x, y))[2000:]))
            rows.append((thr, rat, m))
        print('thr %5.1f: ' % thr + ' '.join('%.2f' % r[2] for r in rows[-len(rats):]), flush=True)
    csv('makeup.csv', 'threshold,ratio,makeup_db', rows)
    RES['makeup'] = {'ratios': rats, 'rows': [list(map(float, r)) for r in rows]}
    save()


def sec_knee():
    """The static curve in 0.5 dB steps (alternating signal: the detector sees the level exactly)."""
    levels = list(np.arange(-70.0, 0.01, 0.5))
    out = {}
    rows = []
    for thr in (-6.0, -12.0, -24.0, -36.0, -48.0):
        for rat in (1.5, 2.0, 4.0, 50.0):
            x = alt(levels, [0.12] * len(levels))
            y = hc(x, threshold=thr, ratio=rat, attack=0.5, release=10.0, punch=0.0)[0]
            g = db(gain_trace(x, y))
            n = int(0.12 * FS)
            gs = np.array([np.median(g[(i + 1) * n - 1200:(i + 1) * n]) for i in range(len(levels))])
            out['%g/%g' % (thr, rat)] = gs.tolist()
            for L, v in zip(levels, gs): rows.append((thr, rat, L, v))
            print('thr %g ratio %g done' % (thr, rat), flush=True)
    RES['knee'] = {'levels': levels, 'curves': out}
    csv('knee.csv', 'threshold,ratio,level_peak_db,gain_db', rows)
    save()


def sec_mix():
    """Mix: output against the dry and the fully compressed signal (alternating signal, steady)."""
    r = {}
    x = alt([-6.0], [1.0])
    g100 = np.median(gain_trace(x, hc(x, threshold=-30.0, ratio=4.0, mix=100.0)[0])[FS // 2:])
    for m in (0.0, 10.0, 25.0, 50.0, 75.0, 90.0, 100.0):
        g = np.median(gain_trace(x, hc(x, threshold=-30.0, ratio=4.0, mix=m)[0])[FS // 2:])
        lin = (1 - m / 100) + m / 100 * g100
        r[str(m)] = {'gain': float(g), 'linear_blend': float(lin)}
        print('mix %5.1f gain %.5f  linear blend of dry and wet %.5f' % (m, g, lin), flush=True)
    RES['mix'] = r
    save()


def sec_output():
    """Output gain, and the clipper (Limiter Off) or limiter (On) at the output."""
    r = {}
    x = alt([-20.0], [0.5])
    for o in (-18.0, -6.0, 0.0, 6.0, 18.0):
        g = np.median(db(gain_trace(x, hc(x, threshold=0.0, ratio=1.0, output=o)[0]))[FS // 4:])
        r['gain %g' % o] = float(g)
    print('output gains', r, flush=True)
    for lim in ('Off', 'On'):
        for lvl in (-6.0, 0.0, 6.0, 12.0):
            x = sine(1000, 1.0, 10 ** (-6 / 20))
            y = hc(x, threshold=0.0, ratio=1.0, output=lvl + 6.0, limiter=lim)[0]
            pk = float(np.max(np.abs(y[FS // 2:])))
            h3 = float(db(amp_at(y, 3000, FS // 2, FS)) - db(amp_at(y, 1000, FS // 2, FS)))
            r['%s %g' % (lim, lvl)] = {'peak': pk, 'peak_db': float(db(pk)), 'h3_db': h3}
            print('limiter %s, would-be peak %+5.1f dB: out peak %.5f (%.2f dB), h3 %.1f dB' % (lim, lvl, pk, db(pk), h3), flush=True)
    # the limiter in time: a burst 12 dB over, then back down
    x = alt([-12.0, 0.0, -12.0], [0.3, 0.3, 1.0])
    g = db(gain_trace(x, hc(x, threshold=0.0, ratio=1.0, output=12.0, limiter='On')[0]))
    np.save(os.path.join(OUT, 'limiter_burst.npy'), g.astype(np.float32))
    t1 = int(0.6 * FS)
    rel = g[t1:] - g[-1]
    r['limiter_gain_in_burst_db'] = float(np.median(g[int(0.45 * FS):t1]))
    r['limiter_release_t63_ms'] = float(np.argmax(rel > rel[0] * (1 - 0.632)) / FS * 1000) if rel[0] < -0.01 else 0.0
    print('limiter: gain in the burst %.2f dB, release t63 %.1f ms' % (r['limiter_gain_in_burst_db'], r['limiter_release_t63_ms']), flush=True)
    RES['output'] = r
    save()


def sec_analog():
    """Analog modes: small-signal response (log sweep, averaged), harmonics against level, the noise floor."""
    r = {}
    n = 4 * FS
    f0, f1 = 10.0, 23000.0
    t = np.arange(n) / FS
    K = n / FS / np.log(f1 / f0)
    sweep = np.sin(2 * np.pi * f0 * K * (np.exp(t / K) - 1)).astype(np.float32)
    for an in (1, 2, 3, 4):
        res = {}
        for lvl in (-40.0, -20.0):
            x = (10 ** (lvl / 20) * sweep).astype(np.float32)
            y = np.mean([hc(x, threshold=0.0, ratio=1.0, analog=an)[0] for _ in range(2)], axis=0)
            X = np.fft.rfft(x, 2 * n); Y = np.fft.rfft(y, 2 * n)
            fr = np.fft.rfftfreq(2 * n, 1 / FS)
            H = Y / np.where(np.abs(X) > 1e-9, X, 1e-9)
            pts = [20, 30, 50, 80, 100, 150, 200, 300, 500, 1000, 2000, 4000, 8000, 12000, 16000, 20000]
            res['resp %g' % lvl] = {str(f): float(db(np.mean(np.abs(H[(fr > f / 1.06) & (fr < f * 1.06)])))) for f in pts}
        for f in (100.0, 1000.0):
            for lvl in (-40.0, -20.0, -10.0, -3.0, 0.0):
                y = hc(sine(f, 1.0, 10 ** (lvl / 20)), threshold=0.0, ratio=1.0, analog=an)[0]
                a = [amp_at(y, f * h, FS // 2, FS) for h in range(1, 6)]
                res['h %g %g' % (f, lvl)] = [float(db(a[0]) - lvl)] + [float(db(v) - db(a[0])) for v in a[1:]]
        y = hc(np.zeros(4 * FS, np.float32), threshold=0.0, ratio=1.0, analog=an)[0][FS:]
        fq, P = ss.welch(y, FS, nperseg=8192)
        res['noise_rms_db'] = float(db(np.sqrt(np.mean(y.astype(np.float64) ** 2))))
        bands = [(20, 100), (100, 300), (300, 1000), (1000, 3000), (3000, 10000), (10000, 20000)]
        res['noise_bands_db'] = {'%d-%d' % b: float(10 * np.log10(np.sum(P[(fq >= b[0]) & (fq < b[1])]) * (fq[1] - fq[0]) + 1e-30)) for b in bands}
        np.save(os.path.join(OUT, 'noise_psd_%d.npy' % an), np.stack([fq, P]).astype(np.float32))
        r[str(an)] = res
        print('analog', an, res['resp -40'], 'noise %.1f dB' % res['noise_rms_db'], flush=True)
        print('   harmonics 1k -10:', ['%.1f' % v for v in res['h 1000 -10']], ' 100 Hz -10:', ['%.1f' % v for v in res['h 100 -10']], flush=True)
    RES['analog'] = r
    save()


def sec_stereo():
    """Channel linking: a loud left and a quiet right channel; and the mono component."""
    r = {}
    xl = alt([-6.0], [1.0]); xr = alt([-40.0], [1.0])
    y = hc(np.stack([xl, xr]), threshold=-30.0, ratio=4.0)
    r['loud_left'] = float(np.median(db(gain_trace(xl, y[0]))[FS // 2:]))
    r['quiet_right'] = float(np.median(db(gain_trace(xr, y[1]))[FS // 2:]))
    r['quiet_alone'] = float(np.median(db(gain_trace(xr, hc(xr, threshold=-30.0, ratio=4.0)[0]))[FS // 2:]))
    ym = R.render_hc(xl, FS, name="H-Comp Mono", threshold=-30.0, ratio=4.0)[0]
    r['mono_loud'] = float(np.median(db(gain_trace(xl, ym))[FS // 2:]))
    print(r, flush=True)
    RES['stereo'] = r
    save()


def sec_rates():
    """Latency, makeup and static gain, attack and release at other sample rates."""
    global FS
    r = {}
    keep = FS
    for fs in (44100, 88200, 96000, 192000):
        FS = fs
        x = alt([-50.0, -10.0, -50.0], [0.5, 0.5, 1.0])
        g = db(gain_trace(x, hc(x, threshold=-30.0, ratio=50.0, attack=7.0, release=100.0, punch=0.0)[0]))
        t0, t1 = int(0.5 * fs), fs
        g0 = np.median(g[t0 - 2000:t0 - 200]); g1 = np.median(g[t1 - 2000:t1 - 200])
        fa = (g[t0:t1] - g0) / (g1 - g0); fr = (g[t1:] - g1) / (np.median(g[-2000:]) - g1)
        first = (np.argmax(np.abs(g[t0 - 400:] - g0) > 0.01) - 400) / fs * 1000
        imp = np.zeros(fs // 2, np.float32); imp[1000] = 0.01
        yi = hc(imp, threshold=0.0, ratio=1.0)[0]
        r[str(fs)] = {'makeup': float(g0), 'gain': float(g1), 'attack_t63_ms': float(np.argmax(fa >= 0.632) / fs * 1000),
                      'release_t63_ms': float(np.argmax(fr >= 0.632) / fs * 1000), 'lookahead_ms': float(-first),
                      'impulse_at': int(np.argmax(np.abs(yi)))}
        print(fs, r[str(fs)], flush=True)
    FS = keep
    RES['rates'] = r
    save()


def sec_sync():
    """Release synced to a tempo: BPM mode (the plug-in's own tempo, default 120) and Host mode (no host tempo here)."""
    r = {}
    for mode in ('BPM', 'Host'):
        for note in ('1/64T', '1/64', '1/32', '1/16D', '1/16', '1/8T', '1/8', '1/4', '1/2', '1B', '2B'):
            x = alt([-10.0, -50.0], [1.0, 4.0])
            try:
                g = db(gain_trace(x, hc(x, threshold=-30.0, ratio=50.0, attack=7.0, punch=0.0, sync=mode, releasebpm=note)[0]))
            except Exception as e:
                print(mode, note, 'error', e)
                continue
            t1 = FS
            g0 = np.median(g[t1 - 2000:t1 - 200]); fr = (g[t1:] - g0) / (np.median(g[-2000:]) - g0)
            r['%s %s' % (mode, note)] = {'t63_ms': float(np.argmax(fr >= 0.632) / FS * 1000), 't90_ms': float(np.argmax(fr >= 0.9) / FS * 1000)}
            print(mode, note, r['%s %s' % (mode, note)], flush=True)
    RES['sync'] = r
    save()


def sec_plots():
    """Plots of the saved traces (dynamics.png) and the knee fit (knee.png)."""
    from scipy import optimize
    fig_, ax = plt.subplots(2, 2, figsize=(11, 7))
    for att in (0.5, 3.0, 7.0, 20.0, 100.0):
        g = np.load(os.path.join(OUT, 'attack_%g.npy' % att))
        t = (np.arange(len(g)) - 480) / FS * 1000
        ax[0, 0].plot(t, g, label='attack %g ms' % att)
    ax[0, 0].set_xlim(-5, 300); ax[0, 0].set_xlabel('ms after the step'); ax[0, 0].set_ylabel('gain (dB)'); ax[0, 0].legend(fontsize=7); ax[0, 0].grid()
    ax[0, 0].set_title('attack: -50 -> -10 dBFS, threshold -30, ratio 50')
    for rel in (3.0, 30.0, 100.0, 300.0, 1000.0):
        g = np.load(os.path.join(OUT, 'release_%g.npy' % rel))
        t = (np.arange(len(g)) - 480) / FS * 1000
        ax[0, 1].plot(t, g, label='release %g ms' % rel)
    ax[0, 1].set_xscale('symlog', linthresh=10); ax[0, 1].set_xlim(-5, 5000); ax[0, 1].set_xlabel('ms after the step'); ax[0, 1].legend(fontsize=7); ax[0, 1].grid()
    ax[0, 1].set_title('release: -10 -> -50 dBFS')
    for pu in (0.0, 1.0, 3.0, 10.0, 20.0):
        g = np.load(os.path.join(OUT, 'punch_0.5_%g.npy' % pu))
        t = (np.arange(len(g)) - 480) / FS * 1000
        ax[1, 0].plot(t, g, label='punch %g' % pu)
    ax[1, 0].set_xlim(-3, 40); ax[1, 0].set_xlabel('ms after the step'); ax[1, 0].set_ylabel('gain (dB)'); ax[1, 0].legend(fontsize=7); ax[1, 0].grid()
    ax[1, 0].set_title('punch delays the detector (attack 0.5 ms)')
    for lim in ('Off', 'On'):
        g = np.load(os.path.join(OUT, 'limiter_%s.npy' % lim))
        t = (np.arange(len(g)) - 480) / FS * 1000
        ax[1, 1].plot(t, g, label='limiter ' + lim)
    ax[1, 1].set_xlim(-2, 4); ax[1, 1].set_xlabel('ms after the burst starts'); ax[1, 1].legend(fontsize=7); ax[1, 1].grid()
    ax[1, 1].set_title('output limiter: look-ahead ramp (a 12 dB burst)')
    fig('dynamics.png')

    lv = np.array(RES['knee']['levels'])
    curves = {k: np.array(v) for k, v in RES['knee']['curves'].items()}

    def model(p, thr, rat, L):
        eps, w, dT = p
        o = 20 * np.log10(10 ** (L / 20) + abs(eps)) - (thr + dT)
        w = abs(w) + 1e-6
        return (1 - 1 / rat) * np.where(o <= -w / 2, 0, np.where(o >= w / 2, o, (o + w / 2) ** 2 / (2 * w)))

    def resid(p):
        return np.concatenate([model(p, *map(float, k.split('/')), lv) - (g[0] - g) for k, g in curves.items()])
    s = optimize.least_squares(resid, [0.001, 3.0, 0.5])
    RES['knee_model'] = {'eps': float(abs(s.x[0])), 'eps_db': float(db(abs(s.x[0]))), 'knee_db': float(abs(s.x[1])),
                         'threshold_offset_db': float(s.x[2]), 'rms_db': float(np.sqrt(np.mean(s.fun ** 2))), 'max_db': float(np.abs(s.fun).max())}
    print('knee model', RES['knee_model'])
    save()
    plt.figure(figsize=(8, 6))
    for k, g in curves.items():
        thr, rat = map(float, k.split('/'))
        if rat not in (1.5, 50.0): continue
        plt.plot(lv, g[0] - g, '.', ms=3)
        plt.plot(lv, model(s.x, thr, rat, lv), 'k-', lw=0.6)
    plt.xlabel('input peak (dBFS)'); plt.ylabel('gain reduction (dB)'); plt.grid()
    plt.title('static gain reduction (dots) and the fitted model (lines), ratios 1.5 and 50')
    fig('knee.png')


SECTIONS = dict(linear=sec_linear, static=sec_static, detector=sec_detector, freqdet=sec_freqdet, timing=sec_timing,
                punch=sec_punch, release_pd=sec_release_pd, makeup=sec_makeup, knee=sec_knee, mix=sec_mix, output=sec_output,
                analog=sec_analog, stereo=sec_stereo, rates=sec_rates, sync=sec_sync, plots=sec_plots)

if __name__ == '__main__':
    for s in (sys.argv[1:] or list(SECTIONS)):
        print('==', s, flush=True)
        SECTIONS[s]()
