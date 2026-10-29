"""Phase 3, the full campaign on the reference CLA Vocals (Stereo component unless said; 48 kHz unless said): every
control at every amount, the level dependence, every pair of stages over a grid of amounts, the tempo, stereo input,
the Mono/Stereo component, the sample rates, the timing and parameter changes, and random combinations / the factory
presets on vocals. Jobs run in parallel workers (one plug-in instance each); each section writes
reference/wvoxchain/meas/campaign/<section>.json (git-ignored); analyse_voxchain.py reads them.

    python -I campaign_voxchain.py <section> ... [--workers 12]
    sections: single levels pairs tempo stereo monostereo rates timing changes combos (or all)
"""
import json
import os
import sys
import time
import numpy as np
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
OUT = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'campaign')
FS = 48000
STAGES = ('bass', 'treble', 'compress', 'reverb', 'delay', 'pitch')
CLEAR = {s + '_color': 0 for s in STAGES}
DEFAULT = {s + '_color': 1 for s in STAGES}
FADER_STEPS = list(range(-10, 11))
GRID = (-10, -5, 0, 5, 10)
# 1/12-octave frequencies for the transfer functions
FREQS = 20.0 * 2 ** (np.arange(0, 120) / 12.0)
FREQS = FREQS[FREQS <= 20000]


# ---------------------------------------------------------------------------------------------------------------------
# signals (deterministic, by name)
def signal(spec, fs=FS):
    kind = spec[0]
    if kind == 'noise':                                             # ('noise', dB rms, secs, seed)
        _, db, secs, seed = spec
        return (10 ** (db / 20) * np.random.default_rng(seed).standard_normal(int(fs * secs))).astype(np.float32)
    if kind == 'impulse':                                           # ('impulse', amplitude, secs)
        x = np.zeros(int(fs * spec[2]), np.float32); x[1000] = spec[1]; return x
    if kind == 'tones':                                             # ('tones', freq, [levels dBFS peak], secs each)
        _, f0, levels, secs = spec
        n = int(fs * secs); t = np.arange(n) / fs
        return np.concatenate([10 ** (l / 20) * np.sin(2 * np.pi * f0 * t) for l in levels]).astype(np.float32)
    if kind == 'noisesteps':                                        # ('noisesteps', [levels dB rms], secs each, seed)
        _, levels, secs, seed = spec
        n = int(fs * secs); r = np.random.default_rng(seed)
        return np.concatenate([10 ** (l / 20) * r.standard_normal(n) for l in levels]).astype(np.float32)
    if kind == 'burst':                                             # ('burst', freq, low dB, high dB, on secs, off secs)
        _, f0, lo, hi, on, off = spec
        n0, n1 = int(fs * 0.5), int(fs * on); n2 = int(fs * off); t = np.arange(n0 + n1 + n2) / fs
        env = np.concatenate([np.full(n0, 10 ** (lo / 20)), np.full(n1, 10 ** (hi / 20)), np.full(n2, 10 ** (lo / 20))])
        return (env * np.sin(2 * np.pi * f0 * t)).astype(np.float32)
    if kind == 'vocal':                                             # ('vocal', part, secs, gain dB)
        import vocal_corpus as V
        x = V.part(spec[1])[0][:int(FS * spec[2])] * 10 ** (spec[3] / 20)
        if fs != FS:
            from scipy.signal import resample_poly
            from math import gcd
            d = gcd(fs, FS); x = resample_poly(x, fs // d, FS // d)
        return x.astype(np.float32)
    if kind == 'stereo':                                            # ('stereo', how, secs): two-channel test inputs
        _, how, secs = spec
        r = np.random.default_rng(11); n = int(fs * secs)
        a = 0.03 * r.standard_normal(n); b = 0.03 * r.standard_normal(n)
        z = np.zeros(n)
        return np.stack({'left': (a, z), 'right': (z, a), 'different': (a, b), 'antiphase': (a, -a)}[how]).astype(np.float32)
    raise ValueError(spec)


# ---------------------------------------------------------------------------------------------------------------------
# rendering in a worker
_C = None


def _init():
    global _C
    import cv_ref as C
    _C = C
    C.plugin()


def host(kw):
    return _C.host(**kw)


def render(spec, kw, noise=False, fs=FS):
    x = signal(spec, fs)
    return x, _C.render(x, fs, {} if noise else {_C.NOISE: 0}, host(kw), latency=None if fs <= 48000 else 385)


def tf(x, y, fs=FS):
    from scipy.signal import csd, welch
    f, pxy = csd(x, y, fs, nperseg=16384)
    _, pxx = welch(x, fs, nperseg=16384)
    h = pxy / np.maximum(pxx, 1e-30)
    hi = np.interp(FREQS, f, h.real) + 1j * np.interp(FREQS, f, h.imag)
    return (20 * np.log10(np.abs(hi) + 1e-12)).round(3).tolist(), np.angle(hi).round(4).tolist()


def step_gains(x, y, nsteps, frac=(2 / 3, 1.0)):
    n = len(x) // nsteps
    out = []
    for i in range(nsteps):
        a, b = i * n + int(frac[0] * n), i * n + int(frac[1] * n)
        out.append(round(float(20 * np.log10(np.sqrt(np.mean(y[a:b] ** 2)) / max(np.sqrt(np.mean(x[a:b] ** 2)), 1e-12) + 1e-12)), 3))
    return out


def wet_stats(w):
    """a wet contribution (2, n), per unit input: energy L / R (dB), first arrival (ms), T60 by T30 (s), L / R
    correlation, its envelope in 10 ms frames (dB)"""
    e = np.sum(w.astype(np.float64) ** 2, 1)
    env = np.max(np.abs(w), 0)
    first = int(np.argmax(env > 0.01 * env.max())) if env.max() > 0 else -1
    s = (w[0] ** 2 + w[1] ** 2)[::-1].cumsum()[::-1]; s = 10 * np.log10(s / max(s[0], 1e-30) + 1e-30)
    i5, i35 = int(np.argmax(s < -5)), int(np.argmax(s < -35))
    fr = 480
    E = np.sum((w[:, :len(env) // fr * fr] ** 2).reshape(2, -1, fr), 2)
    return dict(eL=round(float(10 * np.log10(e[0] + 1e-30)), 3), eR=round(float(10 * np.log10(e[1] + 1e-30)), 3),
                first_ms=round(first / FS * 1000, 2), t60=round(2 * (i35 - i5) / FS, 3) if i35 > i5 else None,
                corr=round(float(np.sum(w[0] * w[1]) / np.sqrt(e[0] * e[1] + 1e-30)), 4),
                envL=(10 * np.log10(E[0] + 1e-20)).round(1).tolist(), envR=(10 * np.log10(E[1] + 1e-20)).round(1).tolist())


_base_cache = {}


def baseline(spec, kw):
    key = json.dumps([spec, sorted(kw.items())])
    if key not in _base_cache: _base_cache[key] = render(spec, kw)[1]
    return _base_cache[key]


def run(job):
    """a job: dict(kind, kw (host settings), spec (signal), ...); returns its measurement"""
    t0 = time.time()
    k = job['kind']
    kw = dict(job.get('kw', {}))
    r = dict(job)
    if k == 'tf':                                                   # the linear transfer function, L and R
        x, y = render(job['spec'], kw)
        r['magL'], r['phL'] = tf(x, y[0]); r['magR'], r['phR'] = tf(x, y[1])
    elif k == 'steps':                                              # a stepped signal's gain per step, L and R
        x, y = render(job['spec'], kw)
        r['gL'] = step_gains(x, y[0], job['n']); r['gR'] = step_gains(x, y[1], job['n'])
    elif k == 'wet':                                                # the contribution over a baseline setting
        x, y = render(job['spec'], kw)
        y0 = baseline(job['spec'], job['base'])
        w = (y - y0)[:, 1000:] / job['spec'][1]
        r.update(wet_stats(w))
        if job.get('keep'):                                         # (the first 0.5 s kept, for shape comparisons)
            r['ir'] = w[:, :int(0.5 * FS)].round(6).tolist()
    elif k == 'envelope':                                           # the output / input level in 1 ms frames
        x, y = render(job['spec'], kw)
        fr = FS // 1000
        n = len(x) // fr * fr
        ex = np.sqrt(np.mean(x[:n].reshape(-1, fr) ** 2, 1)); ey = np.sqrt(np.mean(y[0, :n].reshape(-1, fr) ** 2, 1))
        r['env'] = (20 * np.log10(ey / np.maximum(ex, 1e-12) + 1e-12)).round(2).tolist()
    elif k == 'stats':                                              # a long signal's output: levels, spectrum, L / R
        x, y = render(job['spec'], kw)
        from scipy.signal import welch
        f, pl = welch(y[0], FS, nperseg=8192); _, pr = welch(y[1], FS, nperseg=8192); _, px = welch(x if x.ndim == 1 else x[0], FS, nperseg=8192)
        b = [np.argmin(abs(f - q)) for q in FREQS[::4]]
        r['rms'] = [round(float(20 * np.log10(np.sqrt(np.mean(y[c] ** 2)) + 1e-12)), 3) for c in (0, 1)]
        r['rms_in'] = round(float(20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)), 3)
        r['specL'] = (10 * np.log10(pl[b] / np.maximum(px[b], 1e-30) + 1e-30)).round(2).tolist()
        r['specR'] = (10 * np.log10(pr[b] / np.maximum(px[b], 1e-30) + 1e-30)).round(2).tolist()
        r['corr'] = round(float(np.corrcoef(y[0], y[1])[0, 1]), 4)
        r['peak'] = round(float(20 * np.log10(np.abs(y).max() + 1e-12)), 3)
    elif k == 'stereo':                                             # a two-channel input: each output channel's TF from each input
        x, y = render(job['spec'], kw)
        r['LfromL'] = tf(x[0], y[0])[0] if x[0].std() > 0 else None
        r['RfromR'] = tf(x[1], y[1])[0] if x[1].std() > 0 else None
        r['RfromL'] = tf(x[0], y[1])[0] if x[0].std() > 0 else None
        r['LfromR'] = tf(x[1], y[0])[0] if x[1].std() > 0 else None
        r['rms'] = [round(float(20 * np.log10(np.sqrt(np.mean(y[c] ** 2)) + 1e-12)), 3) for c in (0, 1)]
    elif k == 'rate':                                               # another sample rate: the TF (noise) and the latency
        fs = job['fs']
        x, y = render(job['spec'], kw, fs=fs)
        from scipy.signal import csd, welch
        f, pxy = csd(x, y[0], fs, nperseg=16384); _, pxx = welch(x, fs, nperseg=16384)
        h = pxy / np.maximum(pxx, 1e-30)
        hi = np.interp(FREQS, f, h.real) + 1j * np.interp(FREQS, f, h.imag)
        r['magL'] = (20 * np.log10(np.abs(hi) + 1e-12)).round(3).tolist()
    r['secs'] = round(time.time() - t0, 1)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# the sections' jobs
def kwset(base, **over):
    d = dict(base); d.update(over); return d


TF_SPEC = ('noise', -40.0, 3.0, 1)
IMP = lambda secs: ('impulse', 0.1, secs)
LEVELS = list(range(-45, 7, 3))
TONE = lambda f0: ('tones', f0, LEVELS, 0.5)
NOISE_LEVELS = list(range(-42, 1, 3))
NSTEPS = ('noisesteps', NOISE_LEVELS, 0.6, 3)


def jobs_single():
    J = []
    for st in ('bass', 'treble'):
        for c in (1, 2, 3):
            for v in FADER_STEPS:
                J.append(dict(section='single', kind='tf', stage=st, colour=c, fader=v, spec=TF_SPEC, kw=kwset(CLEAR, **{st + '_color': c, st: v})))
    J.append(dict(section='single', kind='tf', stage='clear', spec=TF_SPEC, kw=CLEAR))
    J.append(dict(section='single', kind='tf', stage='default', spec=TF_SPEC, kw=kwset(DEFAULT, reverb_color=0, delay_color=0, pitch_color=0)))
    for c in (0, 1, 2, 3):
        for v in (FADER_STEPS if c else (0,)):
            for f0 in ((1000, 100, 6000) if v in (-10, 0, 10) else (1000,)):
                J.append(dict(section='single', kind='steps', stage='compress', colour=c, fader=v, freq=f0, n=len(LEVELS), spec=TONE(f0),
                              kw=kwset(CLEAR, compress_color=c, compress=v)))
    for st, secs in (('reverb', 7.0), ('delay', 4.0), ('pitch', 1.0)):
        for c in (1, 2, 3):
            for v in FADER_STEPS:
                J.append(dict(section='single', kind='wet', stage=st, colour=c, fader=v, spec=IMP(secs), base=CLEAR, keep=(v == 0),
                              kw=kwset(CLEAR, **{st + '_color': c, st: v})))
    for name in ('sensitivity', 'output'):
        for v in FADER_STEPS:
            J.append(dict(section='single', kind='steps', stage=name, fader=v, n=len(NOISE_LEVELS), spec=NSTEPS, kw=kwset(DEFAULT, reverb_color=0, delay_color=0, pitch_color=0, **{name: v})))
    for v in np.arange(-6, 6.01, 0.5):
        J.append(dict(section='single', kind='stats', stage='balance', fader=float(v), spec=('noise', -30.0, 2.0, 5), kw=kwset(CLEAR, balance=float(v))))
    return J


def jobs_levels():
    J = []
    for c in (0, 1, 2, 3):
        for sv in GRID:
            J.append(dict(section='levels', kind='steps', what='compress x sensitivity', colour=c, sens=sv, n=len(NOISE_LEVELS), spec=NSTEPS,
                          kw=kwset(CLEAR, compress_color=c, sensitivity=sv)))
    for st in ('bass', 'treble'):
        for c in (1, 2, 3):
            for v in (-10, 10):
                for f0 in ((60, 100, 1000) if st == 'bass' else (1000, 5000, 10000)):
                    for cc in (0, 2):
                        J.append(dict(section='levels', kind='steps', what='eq at level', stage=st, colour=c, fader=v, freq=f0, comp=cc, n=len(LEVELS),
                                      spec=TONE(f0), kw=kwset(CLEAR, **{st + '_color': c, st: v, 'compress_color': cc})))
    for f0 in (2000, 4000, 6000, 10000):
        for sv in GRID:
            J.append(dict(section='levels', kind='steps', what='deesser x sensitivity', freq=f0, sens=sv, n=len(LEVELS), spec=TONE(f0), kw=kwset(CLEAR, sensitivity=sv)))
    for c in (1, 2, 3):                                                 # the wet stages at level: is their send linear?
        for st in ('reverb', 'delay', 'pitch'):
            for amp in (0.01, 0.3, 1.0):
                J.append(dict(section='levels', kind='wet', what='wet at level', stage=st, colour=c, amp=amp, spec=('impulse', amp, 4.0 if st != 'pitch' else 1.0),
                              base=CLEAR, kw=kwset(CLEAR, **{st + '_color': c})))
    return J


def jobs_pairs():
    J = []
    for cb in (1, 2, 3):                                                # Bass x Treble (linear: does the product hold?)
        for ct in (1, 2, 3):
            for vb in GRID:
                for vt in GRID:
                    J.append(dict(section='pairs', kind='tf', pair='bass x treble', cb=cb, ct=ct, vb=vb, vt=vt, spec=TF_SPEC,
                                  kw=kwset(CLEAR, bass_color=cb, treble_color=ct, bass=vb, treble=vt)))
    for st, f0 in (('bass', 100), ('treble', 5000)):                    # EQ x Compress: is the EQ before the dynamics?
        for c in (1, 2, 3):
            for cc in (1, 2, 3):
                for v in (-10, 0, 10):
                    for vc in (-10, 0, 10):
                        J.append(dict(section='pairs', kind='steps', pair=st + ' x compress', c=c, cc=cc, v=v, vc=vc, freq=f0, n=len(LEVELS), spec=TONE(f0),
                                      kw=kwset(CLEAR, **{st + '_color': c, st: v, 'compress_color': cc, 'compress': vc})))
    WET = ('reverb', 'delay', 'pitch')
    for cc in (0, 1, 2, 3):                                             # Compress x the wet stages (where are the sends?)
        for vc in (-10, 0, 10):
            for st in WET:
                for c in (1, 2, 3):
                    J.append(dict(section='pairs', kind='wet', pair='compress x ' + st, cc=cc, vc=vc, colour=c, spec=IMP(4.0 if st != 'pitch' else 1.0),
                                  base=kwset(CLEAR, compress_color=cc, compress=vc), kw=kwset(CLEAR, compress_color=cc, compress=vc, **{st + '_color': c})))
    for eq, c in (('bass', 1), ('bass', 3), ('treble', 1), ('treble', 2), ('treble', 3)):   # EQ x the wet stages
        for v in (-10, 10):
            for st in WET:
                J.append(dict(section='pairs', kind='wet', pair=eq + ' x ' + st, ce=c, v=v, colour=2, spec=IMP(4.0 if st != 'pitch' else 1.0), keep=True,
                              base=kwset(CLEAR, **{eq + '_color': c, eq: v}), kw=kwset(CLEAR, **{eq + '_color': c, eq: v, st + '_color': 2})))
    import itertools                                                    # the wet stages together: in series or in parallel?
    for cr, cd, cp in itertools.product((0, 1, 2, 3), repeat=3):
        if (cr > 0) + (cd > 0) + (cp > 0) < 1: continue
        J.append(dict(section='pairs', kind='wet', pair='wet combination', cr=cr, cd=cd, cp=cp, spec=IMP(5.0), keep=True,
                      base=CLEAR, kw=kwset(CLEAR, reverb_color=cr, delay_color=cd, pitch_color=cp)))
    for v in (-6, -3, 3, 6):                                            # Balance and Sensitivity x the wet stages
        for st in WET:
            J.append(dict(section='pairs', kind='wet', pair='balance x ' + st, balance=v, colour=2, spec=IMP(4.0 if st != 'pitch' else 1.0),
                          base=kwset(CLEAR, balance=v), kw=kwset(CLEAR, balance=v, **{st + '_color': 2})))
    for v in (-10, 10):
        for st in WET:
            J.append(dict(section='pairs', kind='wet', pair='sensitivity x ' + st, sens=v, colour=2, spec=IMP(4.0 if st != 'pitch' else 1.0),
                          base=kwset(CLEAR, sensitivity=v), kw=kwset(CLEAR, sensitivity=v, **{st + '_color': 2})))
            J.append(dict(section='pairs', kind='wet', pair='output x ' + st, out=v, colour=2, spec=IMP(4.0 if st != 'pitch' else 1.0),
                          base=kwset(CLEAR, output=v), kw=kwset(CLEAR, output=v, **{st + '_color': 2})))
    return J


def jobs_stereo():
    J = []
    configs = {'clear': CLEAR, 'dry default': kwset(DEFAULT, reverb_color=0, delay_color=0, pitch_color=0)}
    for st in ('reverb', 'delay', 'pitch'):
        for c in (1, 2, 3): configs['%s %d' % (st, c)] = kwset(CLEAR, **{st + '_color': c})
    configs['balance -6'] = kwset(CLEAR, balance=-6); configs['balance +6'] = kwset(CLEAR, balance=6)
    for name, kw in configs.items():
        for how in ('left', 'right', 'different', 'antiphase'):
            J.append(dict(section='stereo', kind='stereo', config=name, how=how, spec=('stereo', how, 4.0), kw=kw))
    return J


def jobs_rates():
    J = []
    configs = {'clear': CLEAR, 'bass 1 +10': kwset(CLEAR, bass_color=1, bass=10), 'bass 3 +10': kwset(CLEAR, bass_color=3, bass=10),
               'treble 1 +10': kwset(CLEAR, treble_color=1, treble=10), 'treble 2 +10': kwset(CLEAR, treble_color=2, treble=10),
               'treble 3 +10': kwset(CLEAR, treble_color=3, treble=10), 'bass 1 -10': kwset(CLEAR, bass_color=1, bass=-10)}
    for fs in (44100, 48000, 88200, 96000):
        for name, kw in configs.items():
            J.append(dict(section='rates', kind='rate', fs=fs, config=name, spec=TF_SPEC, kw=kw))
    return J


def jobs_timing():
    J = []
    for c in (1, 2, 3):
        for v in (-10, 0, 10):
            for lo, hi in ((-40, -20), (-30, -6), (-20, 0)):
                for on in (0.05, 0.3, 1.5):
                    J.append(dict(section='timing', kind='envelope', what='compress', colour=c, fader=v, lo=lo, hi=hi, on=on,
                                  spec=('burst', 1000, lo, hi, on, 1.5), kw=kwset(CLEAR, compress_color=c, compress=v)))
    for f0 in (6000, 10000):
        for lo, hi in ((-30, -10), (-30, -3), (-20, 0)):
            for on in (0.02, 0.3):
                J.append(dict(section='timing', kind='envelope', what='deesser', freq=f0, lo=lo, hi=hi, on=on,
                              spec=('burst', f0, lo, hi, on, 0.6), kw=CLEAR))
    return J


def factory_presets():
    import re
    path = r'C:\Program Files (x86)\Waves\Plug-Ins V17\CLA Vocals.bundle\Contents\Resources\Presets\1000.xml'
    txt = open(path, encoding='utf-8', errors='replace').read()
    out = []
    for i, m in enumerate(re.finditer(r'<Preset[^>]*Name="([^"]*)".*?<Parameters Type="RealWorld">([^<]*)</Parameters>', txt, re.S)):
        v = m.group(2).split()
        if len(v) < 67: continue
        kw = {}
        for j, st in enumerate(STAGES):
            if v[54 + 2 * j] != '*': kw[st] = float(v[54 + 2 * j])
            if v[55 + 2 * j] != '*': kw[st + '_color'] = int(float(v[55 + 2 * j]))
        if v[66] != '*': kw['balance'] = float(v[66])
        out.append(('preset %02d' % i, kw))                          # (the names stay out of the project: numbered)
    return out


def jobs_combos():
    J = []
    rng = np.random.default_rng(2026)
    for i in range(120):                                                # random settings of every control
        kw = {st: round(float(rng.uniform(-10, 10)), 1) for st in STAGES}
        kw.update({st + '_color': int(rng.integers(0, 4)) for st in STAGES})
        kw['sensitivity'] = round(float(rng.uniform(-10, 10)), 1); kw['output'] = round(float(rng.uniform(-10, 10)), 1)
        kw['balance'] = round(float(rng.uniform(-6, 6)), 1)
        J.append(dict(section='combos', kind='stats', combo='random %03d' % i, part=24 + i % 12, spec=('vocal', 24 + i % 12, 12.0, 0.0), kw=kw))
    for name, kw in factory_presets():
        J.append(dict(section='combos', kind='stats', combo=name, part=24, spec=('vocal', 24, 12.0, 0.0), kw=kw))
    return J


SECTIONS = {'single': jobs_single, 'levels': jobs_levels, 'pairs': jobs_pairs, 'stereo': jobs_stereo, 'rates': jobs_rates,
            'timing': jobs_timing, 'combos': jobs_combos}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    workers = 12
    for a in sys.argv[1:]:
        if a.startswith('--workers'): workers = int(a.split('=')[1]) if '=' in a else workers
    if args == ['all']: args = list(SECTIONS)
    os.makedirs(OUT, exist_ok=True)
    for sec in args:
        jobs = SECTIONS[sec]()
        t0 = time.time()
        print('%s: %d jobs on %d workers' % (sec, len(jobs), workers), flush=True)
        res = []
        with Pool(workers, initializer=_init) as pool:
            for i, r in enumerate(pool.imap_unordered(run, jobs, chunksize=1)):
                res.append(r)
                if (i + 1) % 25 == 0 or i + 1 == len(jobs):
                    print('  %d / %d  (%.0f s)' % (i + 1, len(jobs), time.time() - t0), flush=True)
        json.dump(res, open(os.path.join(OUT, sec + '.json'), 'w'))
        print('%s done in %.0f s' % (sec, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
