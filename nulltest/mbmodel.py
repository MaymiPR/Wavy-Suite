"""WBassX model pieces identified on the reference (Python, float64), shared by the identification scripts."""
import numpy as np
import scipy.signal as ss
FS = 48000
Q2 = 2 ** -0.5


def sos1(fc, high, fs=FS):
    k = np.tan(np.pi * fc / fs); n = 1 / (1 + k)
    b = [n, -n, 0] if high else [k * n, k * n, 0]
    return np.array(b + [1, (k - 1) * n, 0])


def sos2(fc, q, high, fs=FS):
    k = np.tan(np.pi * fc / fs); k2 = k * k; n = 1 / (1 + k / q + k2)
    b = [n, -2 * n, n] if high else [k2 * n, 2 * k2 * n, k2 * n]
    return np.array(b + [1, 2 * (k2 - 1) * n, (1 - k / q + k2) * n])


def lp3(F, fs=FS): return np.stack([sos1(F, False, fs), sos2(F, 1.0, False, fs)])
def hp3(F, fs=FS): return np.stack([sos1(F, True, fs), sos2(F, 1.0, True, fs)])


def g_highpass(F, hp, fs=FS):
    """the generated part's high-pass (measured): 6 dB/oct setting HP2(16)^2, 12: HP2(F) HP2(16), 24: HP2(0.8 F)^2"""
    if hp == 0: return np.stack([sos2(16, Q2, True, fs)] * 2)
    if hp == 1: return np.stack([sos2(F, Q2, True, fs), sos2(16, Q2, True, fs)])
    return np.stack([sos2(0.8 * F, Q2, True, fs)] * 2)


def g_small_signal(F, hp, fs=FS):
    """G at rest (very low level), up to the gain K(F): -LP3(F) * high-pass"""
    return np.concatenate([lp3(F, fs), g_highpass(F, hp, fs)])


def b_highpass_freq(F):
    """the product's second input: HP2 at F/3, held within 20 ... 40 Hz (two-tone kernels, Freq 50 ... 256)"""
    return min(max(F / 3.0, 20.0), 40.0)


import json as _json, os as _os
_KT = None
def k_out(F):
    """the output scale of G per Freq setting (measured table, whole Hz 32 ... 256)"""
    global _KT
    if _KT is None:
        t = _json.load(open(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "measurements", "maxxbass", "k_out_table.json")))
        _KT = {int(k): v[0] for k, v in t.items()}
    return _KT[int(round(min(max(F, 32), 256)))]


try:
    import ctypes as _ct
    _fast = _ct.CDLL(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "mbfast.dll"))
    _fast.mb_generator.argtypes = [_ct.c_void_p, _ct.c_void_p, _ct.c_int] + [_ct.c_double] * 7
except OSError:
    _fast = None


def generator(w, F, decay_db, mode="implicit", fs=FS):
    """y = w + 8 D w HP_B(y), sample by sample (the Decay feedback). mode: implicit (same sample), delay1 (HP_B of y up
    to n-1), series2 (second order only)"""
    a = 8.0 * 10 ** (decay_db / 20)
    s = sos2(b_highpass_freq(F), Q2, True, fs)
    b0, b1, b2, a1, a2 = s[0], s[1], s[2], s[4], s[5]
    if mode == "prev":       # y[n] = w[n] (1 + a v[n-1]), v = HP_B(y), y within +-1 (the form used by WBassX)
        y = np.zeros_like(w); z1 = z2 = vp = 0.0
        for n in range(len(w)):
            yn = min(max(w[n] * (1 + a * vp), -1.0), 1.0)
            v = b0 * yn + z1; z1 = b1 * yn - a1 * v + z2; z2 = b2 * yn - a2 * v; vp = v; y[n] = yn
        return y
    if mode == "series2":
        return w + a * w * ss.sosfilt(np.atleast_2d(s), w)
    if mode == "implicit" and _fast is not None:
        w = np.ascontiguousarray(w, np.float64); y = np.empty_like(w)
        _fast.mb_generator(w.ctypes.data, y.ctypes.data, len(w), a, s[0], s[1], s[2], s[4], s[5], 0.2)
        return y
    y = np.zeros_like(w); z1 = z2 = 0.0; prev = 0.0
    for n in range(len(w)):
        if mode == "implicit":
            # v = b0 y + z1 ; y = w (1 + a v)  ->  y = w (1 + a z1) / (1 - a b0 w)
            yn = w[n] * (1 + a * z1) / max(1 - a * b0 * w[n], 0.2)     # guard against the pole (not reached by the reference)
            v = b0 * yn + z1
        else:
            v = z1 + b0 * 0.0  # HP_B output from samples up to n-1 only (the state)
            yn = w[n] * (1 + a * v)
            v = b0 * yn + z1
        z1 = b1 * yn - a1 * v + z2
        z2 = b2 * yn - a2 * v
        y[n] = yn
    return y


def g_from_w(w, F, hp, decay_db, mode="implicit", fs=FS):
    y = generator(w, F, decay_db, mode, fs)
    return -k_out(F) * ss.sosfilt(g_highpass(F, hp, fs), y)
