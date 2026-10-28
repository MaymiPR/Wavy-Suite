"""Phase 3 helper: renders the reference CLA Vocals (Stereo component) through pedalboard with any of its 68 preset-array
values set through the state (nulltest/wavesstate.py), so the chain's children can be measured one at a time through
their bypass slots. The output is aligned (the 193-sample latency at 44.1 / 48 kHz removed).

    import cv_ref as C
    y = C.render(x, 48000, {C.BASS: 5, C.only('eq'): ...})
"""
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pedalboard as pb
import wavesstate as W
import refhost as R

NAME = 'CLA Vocals Stereo'
# the preset array (ParamXML 1000.xml, RESEARCH.md WVox Chain)
SENS, DRY_OUT, WET_IN, WET_OUT, OUTPUT = 0, 1, 2, 3, 4
DRY_MS, MAIN_MUTE, LEFT_PAN = 19, 20, 21
BYPASS_ALL = 39
CHILDREN = {'gain1': 40, 'ssl': 41, 'eq': 42, 'l1': 43, 'c1176': 44, 'dess1': 45, 'dess2': 46, 'rvb': 47, 'gain2': 48,
            'hdly': 49, 'stap': 50, 'gain3': 51, 'dbl': 52, 'gain4': 53}
BASS, BASS_C, TREBLE, TREBLE_C, COMP, COMP_C, REVERB, REVERB_C, DELAY, DELAY_C, PITCH, PITCH_C = range(54, 66)
BALANCE, NOISE = 66, 67

_plugin = None
_base = None


def plugin():
    global _plugin, _base
    if _plugin is None:
        _plugin = pb.load_plugin(R.WAVESHELL, plugin_name=NAME)
        _base = _plugin.raw_state
    return _plugin


def defaults():
    plugin()
    return W.get_values(_base)


def only(*names):
    """bypass values that leave only the named children running"""
    return {i: (0 if n in names else 1) for n, i in CHILDREN.items()}


# the host's parameters (normalised values): the faders (value + 10) / 20, Balance (value + 6) / 12, a colour its
# index / 3 (0 clear, 1 .. 3 the flavours)
FADERS = ('sensitivity', 'bass', 'treble', 'compress', 'reverb', 'delay', 'pitch', 'output')
COLOURS = ('bass_color', 'treble_color', 'compress_color', 'reverb_color', 'delay_color', 'pitch_color')
DRY = {'reverb_color': 0, 'delay_color': 0, 'pitch_color': 0}            # the wet stages muted (their clear position)


def host(**kw):
    """host parameter values in real units: faders and Balance in their units, colours as 0 .. 3"""
    out = {}
    for k, v in kw.items():
        if k in COLOURS: out[k] = v / 3.0
        elif k == 'balance': out[k] = (v + 6.0) / 12.0
        else: out[k] = (v + 10.0) / 20.0
    return out


def render(x, fs=48000, values=None, params=None, preroll=1.0, latency=None):
    """x mono or (2, n); values {index: value} written into the state (only some slots take: Noise does, the children's
    bypasses and the colours do not); params {host name: normalised} (see host()); a preroll of silence (s) is
    rendered first, after the parameters are set, and dropped"""
    p = plugin()
    v = list(W.get_values(_base))
    for k, val in (values or {}).items(): v[k] = val
    W.apply_values(p, v, _base)
    p.reset()
    for k in FADERS + COLOURS + ('balance',):
        p.parameters[k].raw_value = (params or {}).get(k, 0.5 if k in FADERS or k == 'balance' else 1.0 / 3.0)
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x])
    lat = latency if latency is not None else (193 if fs <= 48000 else 385)
    pre = int(preroll * fs)
    xin = np.concatenate([np.zeros((2, pre), np.float32), x, np.zeros((2, lat), np.float32)], 1)
    y = p.process(xin, fs, buffer_size=512, reset=False)
    off = pre + lat - (xin.shape[1] - y.shape[1])            # (pedalboard drops what it compensates of the latency)
    return y[:, off:off + x.shape[1]]


def tf(x, y, fs, nfft=8192):
    """the transfer function (magnitude dB, phase) of y over x by averaged cross spectra (Welch)"""
    from scipy.signal import csd, welch
    f, pxy = csd(x, y, fs, nperseg=nfft)
    _, pxx = welch(x, fs, nperseg=nfft)
    h = pxy / np.maximum(pxx, 1e-20)
    return f, 20 * np.log10(np.abs(h) + 1e-12), np.angle(h)
