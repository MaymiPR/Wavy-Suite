"""Private reference host: renders audio through the installed Waves RVox (WaveShell VST3) with
pedalboard, for local analysis only. Nothing rendered here is committed (see .gitignore)."""
import os
import numpy as np
import pedalboard as pb

WAVESHELL = r"C:\Program Files\Common Files\VST3\WaveShell1-VST3 17.1_x64.vst3"
_cache = {}
STATS = {"silent_retries": 0}

def load(name="RVox Mono"):
    if name not in _cache:
        _cache[name] = pb.load_plugin(WAVESHELL, plugin_name=name)
    return _cache[name]

def setp(p, comp=0.0, gate=None, gain=0.0):
    p.compression = float(comp)
    p.gain = float(gain)
    p.gate = float("-inf") if gate is None else float(gate)

def render(x, fs, comp=0.0, gate=None, gain=0.0, name="RVox Mono", buffer=512, preroll=0.0):
    """x: (channels, n) float32. Returns the output (same shape). A zero preroll (s) is rendered first and dropped."""
    p = load(name)
    setp(p, comp, gate, gain)
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = x[None, :]
    if preroll > 0:
        x = np.concatenate([np.zeros((x.shape[0], int(preroll * fs)), np.float32), x], axis=1)
    for attempt in range(8):
        y = p(x, fs, buffer_size=buffer, reset=True)
        if not _dropout(x, y, fs):
            break
        STATS["silent_retries"] += 1
    return y[:, int(preroll * fs):] if preroll > 0 else y


def _dropout(x, y, fs, win=0.01):
    """The WaveShell now and then returns blocks of pure silence while the input is not silent; detect them."""
    n = max(1, int(win * fs))
    m = x.shape[1] // n
    xi = np.abs(x[:, : m * n]).reshape(x.shape[0], m, n).max(axis=2)
    yo = np.abs(y[:, : m * n]).reshape(y.shape[0], m, n).max(axis=2)
    skip = 8   # the first windows hold the latency / start-up
    return bool(np.any((xi[:, skip:] > 1e-5) & (yo[:, skip:] < 1e-9)))


def tone(fs, f, secs, db):
    t = np.arange(int(secs * fs)) / fs
    return (10 ** (db / 20) * np.sin(2 * np.pi * f * t)).astype(np.float32)

def rms_db(x):
    x = np.asarray(x, np.float64)
    return 10 * np.log10(max(float(np.mean(x * x)), 1e-30))


# ---- RBass ----------------------------------------------------------------------------------------------------
# preset-array indices (ParamXML order): 0 Input, 1 Freq, 2 Response, 3 Ratio, 4 Clip, 5 Intensity (MB gain),
# 6 Hi Pass (Original Bass switch), 7 Out gain, 8 Decay, 13 SmoothTc, 14 Tc, 15 XFadeTc, 16 Smooth
_rbass_base = {}

def render_rbass(x, fs, freq=80.0, intensity=0.0, gain=0.0, hp=0, extra=None, name="RBass Stereo", buffer=512):
    """Render through RBass with every parameter (also the non-automatable ones) set through the plugin state."""
    import wavesstate as W
    p = load(name)
    if name not in _rbass_base:
        _rbass_base[name] = p.raw_state
    base = _rbass_base[name]
    # hidden values (Original Bass switch, Response, Ratio, ...) through the state, visible ones as parameters
    want_hidden = {6: hp, **(extra or {})}
    cur = W.get_values(p.raw_state)
    if any(abs(float(cur[k]) - float(val)) > 1e-6 for k, val in want_hidden.items()):
        v = W.get_values(base)
        for k, val in want_hidden.items(): v[k] = '%g' % val
        W.apply_values(p, v, base)
    p.frequency = float(freq); p.intensity = float(intensity); p.out_gain = float(gain)
    got = W.get_values(p.raw_state)
    assert all(abs(float(got[k]) - float(val)) < 1e-6 for k, val in want_hidden.items()), (want_hidden, got)
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = x[None, :]
    if name.endswith('Stereo') and x.shape[0] == 1: x = np.concatenate([x, x])
    for attempt in range(8):
        y = p(x, fs, buffer_size=buffer, reset=True)
        if not _dropout(x, y, fs):
            break
        STATS["silent_retries"] += 1
    return y


# ---- MaxxBass ---------------------------------------------------------------------------------------------------
# every control is a VST3 parameter; pedalboard snaps values to the plug-in's own steps
MB_DEFAULTS = dict(input_gain=0.0, orig_bass_gain=0.0, harmonics_gain=float("-inf"), ratio=1.0, response=20.0,
                   harmonics_decay=-15.0, highpass=1, frequency=80.0, monitor=0)
_MB_HP = ['6dB/Octave ', '12dB/Octave ', '24dB/Octave ']
_MB_MON = ['Audio ', 'MaxxBass ', 'OrigBass ']


def set_mb(p, **kw):
    v = {**MB_DEFAULTS, **kw}
    for k in ("input_gain", "orig_bass_gain", "harmonics_gain", "harmonics_decay", "response", "frequency"):
        setattr(p, k, float(v[k]))
    p.ratio = '%.2f:1 ' % float(v["ratio"])
    p.highpass = _MB_HP[int(v["highpass"])]
    p.monitor = _MB_MON[int(v["monitor"])]
    return v


def render_mb(x, fs, name="MaxxBass Stereo", buffer=512, **kw):
    """Render through MaxxBass with the given parameters (the rest at their defaults)."""
    p = load(name)
    set_mb(p, **kw)
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = x[None, :]
    if name.endswith('Stereo') and x.shape[0] == 1: x = np.concatenate([x, x])
    for attempt in range(8):
        y = p(x, fs, buffer_size=buffer, reset=True)
        if not _dropout(x, y, fs):
            break
        STATS["silent_retries"] += 1
    return y


# ---- H-Comp ------------------------------------------------------------------------------------------------------
# every user control is a VST3 parameter; measurements default to Analog Off (no noise, no harmonics)
HC_DEFAULTS = dict(threshold=-18.0, ratio=3.0, mix=100.0, attack=7.0, punch=1.0, release=100.0, sync='ms',
                   analog='Off', output=0.0, limiter='Off', releasebpm='1/32D')
_HC_ANALOG = {'Off': 'Off ', 0: 'Off ', 1: 'Mod1 ', 2: 'Mod2 ', 3: 'Mod3 ', 4: 'Mod4 '}


def set_hc(p, **kw):
    v = {**HC_DEFAULTS, **kw}
    p.sync = str(v["sync"]).strip() + ' '          # first: a release set while synced to a note is not kept
    for k in ("threshold", "ratio", "mix", "attack", "punch", "release", "output"):
        setattr(p, k, float(v[k]))
    p.analog = _HC_ANALOG.get(v["analog"], str(v["analog"]).strip() + ' ')
    p.limiter = str(v["limiter"]).strip() + ' '
    p.releasebpm = str(v["releasebpm"]).strip() + ' '
    return v


def render_hc(x, fs, name="H-Comp Stereo", buffer=512, **kw):
    """Render through H-Comp with the given parameters (the rest at HC_DEFAULTS)."""
    p = load(name)
    set_hc(p, **kw)
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = x[None, :]
    if name.endswith('Stereo') and x.shape[0] == 1: x = np.concatenate([x, x])
    for attempt in range(8):
        y = p(x, fs, buffer_size=buffer, reset=True)
        if not _dropout(x, y, fs):
            break
        STATS["silent_retries"] += 1
    return y


REFRENDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "RefRender_artefacts", "Release", "RefRender.exe")


def render_ref(x, fs, name="H-Comp Stereo@-75518996", sc=None, bpm=0.0, params=None, block=512):
    """Render offline through tools/RefRender (a JUCE host): the side-chain bus fed with sc (stereo or mono), a host
    tempo, parameters by display text. Latency removed. Returns (2, n)."""
    import subprocess, tempfile
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x])
    d = tempfile.mkdtemp(prefix="refr_")
    fin, fout, fsc = os.path.join(d, "in.f32"), os.path.join(d, "out.f32"), os.path.join(d, "sc.f32")
    x.T.astype(np.float32).tofile(fin)
    cmd = [REFRENDER, WAVESHELL, name, fin, fout, "fs=%d" % fs, "block=%d" % block]
    if sc is not None:
        sc = np.asarray(sc, np.float32)
        if sc.ndim == 1: sc = np.stack([sc, sc])
        sc.T.astype(np.float32).tofile(fsc); cmd.append("sc=" + fsc)
    if bpm: cmd.append("bpm=%g" % bpm)
    for k, v in (params or {}).items(): cmd.append("%s=%s" % (k, v))
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if not os.path.exists(fout): raise RuntimeError(r.stdout + r.stderr)
    y = np.fromfile(fout, np.float32).reshape(-1, 2).T
    return y, r.stdout
