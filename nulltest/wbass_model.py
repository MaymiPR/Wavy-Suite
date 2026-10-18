"""Python model of the RBass generated path (Phase 4 identification), vectorised with scipy filters.
Low-level structure measured with tones and two-tone kernels (MEASUREMENTS.md):
    b = LP3(F) x                       crossover low band (also the level detector's input)
    u = LP3(F/3) HP3(F) x              band of the generated fundamental
    CS = C0 (1 + E/E0)^-p              upward-compressing gain, E = smoothed |detector input|
    w  = HP3(F) [ k1 CS u + k2 HP3(F/3)(CS u * CS b) ]
"""
import numpy as np
from scipy.signal import butter, sosfilt, lfilter

FS = 48000


def sos(order, kind, fc, fs=FS):
    return butter(order, fc, btype=kind, fs=fs, output='sos')


def sec(kind, order, fc, fs=FS, q=1.0):
    """One section of a Butterworth HP3/LP3 split: first-order, or second-order with Q (1.0 for the 3rd-order Butterworth)."""
    from scipy.signal import bilinear_zpk, zpk2sos
    w = 2 * fs * np.tan(np.pi * fc / fs)                     # prewarped
    if order == 1:
        z, p = ([0.0] if kind == 'high' else []), [-w]
    else:
        r = np.roots([1, w / q, w * w]); z, p = ([0.0, 0.0] if kind == 'high' else []), list(r)
    k = 1.0
    zd, pd, kd = bilinear_zpk(z, p, k if kind == 'high' else w ** order, fs)
    return zpk2sos(zd, pd, kd)


def onepole(x, tau, fs=FS):
    a = np.exp(-1.0 / (tau * fs))
    return lfilter([1 - a], [1, -a], x)


def attrel(x, ta, tr, fs=FS):
    """Peak-style follower: one-pole towards the input with attack ta when rising, release tr when falling."""
    aa = np.exp(-1.0 / (ta * fs)) if ta > 0 else 0.0
    ar = np.exp(-1.0 / (tr * fs))
    xs = x.tolist(); y = [0.0] * len(xs); e = 0.0; ba = 1 - aa; br = 1 - ar
    for i, v in enumerate(xs):
        e = aa * e + ba * v if v > e else ar * e + br * v
        y[i] = e
    return np.asarray(y)


DEFAULT = dict(C0=300.0, E0=0.0175, p=0.5, k1=-1.0, k2=-0.0064, tau1=0.024, tau2=0.02, det="b", smooth="1pole",
               ta=0.001, tr=0.1, gain_pos="post", det_order=3, det_c=1.0)


def generated2(x, F, prm, fs=FS, parts=False):
    """Two gain stages: an upward compressor on l (detector |l|, one-pole tau1) ahead of the high-passes, and an output
    stage driven by the generated output's own level (detector |w|, one-pole tau2)."""
    P = prm
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    d1 = {"l": l, "b": b}[P.get("det1", "l")]
    if P.get("st1", "1pole") == "1pole":
        E1 = onepole(np.abs(d1), P["tau1"], fs)
    else:
        E1 = attrel(np.abs(d1), P["ta1"], P["tau1"], fs)
    CS1 = P["C0"] * (1 + E1 / P["E01"]) ** (-P["p1"])
    n = sosfilt(sos(3, 'high', F, fs), CS1 * l)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS1 * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if P.get("st2", "1pole") == "1pole":
        E2 = onepole(np.abs(w), P["tau2"], fs)
    else:
        E2 = attrel(np.abs(w), P["ta2"], P["tau2"], fs)
    G2 = (1 + E2 / P["E02"]) ** (-P["p2"])
    if parts:
        return G2 * w, dict(l=l, b=b, CS1=CS1, G2=G2, w=w)
    return G2 * w


def generated(x, F, prm=None, fs=FS, parts=False):
    P = dict(DEFAULT); P.update(prm or {})
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    u = sosfilt(sos(3, 'high', F, fs), l)
    if P["det"] == "ap":
        dd = sosfilt(sos(3, 'low', F / 3, fs), b) + sosfilt(sos(3, 'high', F / 3, fs), b)
    else:
        dd = None
    if P["det"] == "lp":
        dd = sosfilt(sos(int(P["det_order"]), 'low', F * P["det_c"], fs), x)
    d = {"b": b, "u": u, "x": x, "l": l, "ap": dd, "lp": dd}[P["det"]]
    r = np.abs(d)
    if P["smooth"] == "inst":              # patent style: gain law on the instantaneous level, then smoothed
        c = (1 + (d / P["E0"]) ** 2) ** (-P["p"] / 2)
        CS = P["C0"] * onepole(c, P["tau1"], fs)
        E = None
    elif P["smooth"] == "1pole":
        E = onepole(r, P["tau1"], fs)
    elif P["smooth"] == "shelf":           # one pole plus a zero: (1 + s tz) / (1 + s tau1), bilinear
        from scipy.signal import bilinear
        bz, az = bilinear([P["tz"], 1.0], [P["tau1"], 1.0], fs)
        E = lfilter(bz, az, r)
    elif P["smooth"] == "mix":             # direct path plus one-pole: (1 - g) * onepole + g * r
        E = (1 - P["g"]) * onepole(r, P["tau1"], fs) + P["g"] * onepole(r, P["tau2"], fs)
    elif P["smooth"] == "2pole":
        E = onepole(onepole(r, P["tau1"], fs), P["tau2"], fs)
    else:
        E = attrel(r, P["ta"], P["tr"], fs)
    if P["smooth"] != "inst":
        CS = P["C0"] * (1 + E / P["E0"]) ** (-P["p"])
    gp = P["gain_pos"]
    if gp == "pre":
        n = sosfilt(sos(3, 'high', F, fs), CS * l)
    elif gp == "mid1":      # HP1 section, gain, HP2 (Q 1) section
        n = sosfilt(sec('high', 2, F, fs), CS * sosfilt(sec('high', 1, F, fs), l))
    elif gp == "mid2":      # HP2 (Q 1) section, gain, HP1 section
        n = sosfilt(sec('high', 1, F, fs), CS * sosfilt(sec('high', 2, F, fs), l))
    elif gp == "split":     # normalised gain G = CS/C0 split: G^alpha on the output, G^(1-alpha) on u, G^(2-alpha) on the product
        G = CS / P["C0"]; a = P["alpha"]
        q = sosfilt(sos(3, 'high', F / 3, fs), G ** (2 - a) * u * b)
        w = P["C0"] * G ** a * sosfilt(sos(3, 'high', F, fs), P["k1"] * G ** (1 - a) * u + P["k2"] * P["C0"] * q)
        if parts:
            return w, dict(b=b, u=u, l=l, E=E, CS=CS, q=q)
        return w
    elif gp == "out":       # patent-style: gain inside the harmonic loop and once more on the output
        q = sosfilt(sos(3, 'high', F / 3, fs), CS * u * b)
        w = CS * sosfilt(sos(3, 'high', F, fs), P["k1"] * u + P["k2"] * q)
        if parts:
            return w, dict(b=b, u=u, l=l, E=E, CS=CS, q=q)
        return w
    else:
        n = CS * u
    m = CS * b
    q = sosfilt(sos(3, 'high', F / 3, fs), n * m)
    w = sosfilt(sos(3, 'high', F, fs), P["k1"] * n + P["k2"] * q)
    if parts:
        return w, dict(b=b, u=u, l=l, E=E, CS=CS, q=q)
    return w
