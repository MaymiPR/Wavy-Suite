"""Private reference host: renders audio through the installed Waves RVox (WaveShell VST3) with
pedalboard, for local analysis only. Nothing rendered here is committed (see .gitignore)."""
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
