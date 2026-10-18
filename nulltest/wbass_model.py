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


try:
    import ctypes, os as _os
    _lib = ctypes.CDLL(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "fastdsp.dll"))
    _lib.ar_follow.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_double]
except OSError:
    _lib = None


def attrel(x, ta, tr, fs=FS, y0=0.0):
    """attack / release one-pole follower (compiled helper when available)"""
    aa = np.exp(-1.0 / (ta * fs)) if ta > 0 else 0.0
    ar = np.exp(-1.0 / (tr * fs)) if tr > 0 else 0.0
    if _lib is not None:
        xin = np.ascontiguousarray(x, np.float64); y = np.empty_like(xin)
        _lib.ar_follow(xin.ctypes.data, y.ctypes.data, len(xin), aa, ar, y0)
        return y
    return _attrel_py(x, ta, tr, fs)


def _attrel_py(x, ta, tr, fs=FS):
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


def lp2(x, fc, q, fs=FS):
    """second-order low-pass smoother (bilinear, pre-warped), unity DC gain"""
    from scipy.signal import bilinear_zpk, zpk2sos
    w = 2 * fs * np.tan(np.pi * fc / fs)
    p = np.roots([1, w / q, w * w])
    zd, pd, kd = bilinear_zpk([], p, w * w, fs)
    return sosfilt(zpk2sos(zd, pd, kd), x)


def generated3(x, F, P, fs=FS, parts=False):
    """One gain stage with a second-order smoother:
       E = LP2(fs_c, Q)(|d|), d = LPn(det_c F) x;  CS = C0 (1 + E/E0)^-p
       placement 'pre': n = HP3(F)(CS l); 'post': n = CS HP3(F) l
       q = HP3(F/3)(n * CS b);  w = HP3(F)(-n + k2 q)"""
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    d = sosfilt(sos(int(P.get("det_order", 3)), 'low', F * P.get("det_c", 1.0), fs), x)
    r = np.abs(d) if P.get("rect", "abs") == "abs" else d * d
    E = lp2(r, P["sm_f"], P["sm_q"], fs)
    if P.get("rect", "abs") != "abs":
        E = np.sqrt(np.maximum(E, 0))
    CS = P["C0"] * (1 + np.maximum(E, 0) / P["E0"]) ** (-P["p"])
    if P.get("place", "pre") == "pre":
        n = sosfilt(sos(3, 'high', F, fs), CS * l)
    else:
        n = CS * sosfilt(sos(3, 'high', F, fs), l)
    beta = P.get("beta", 1.0)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS * (CS / P["C0"]) ** (beta - 1.0) * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if parts:
        return w, dict(CS=CS, E=E, l=l, b=b)
    return w


def generated4(x, F, P, fs=FS, parts=False):
    """Two gains on l (both ahead of the high-passes):
       G_A = (1 + E_A / EA0)^-pA, E_A = one-pole(|b|, tauA)                (slow level, LP3(F) band)
       G_B = one-pole( (1 + (l / LB0)^2)^(-pB/2), tauB )                  (instantaneous law on l, then smoothed;
                                                                            MaxxBass patent: a rational function of
                                                                            the squared input, no rectifier)
       CS = C0 G_A G_B;  n = HP3(F)(CS l);  q = HP3(F/3)(n * CS b);  w = HP3(F)(-n + k2 q)"""
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    EA = onepole(np.abs(b), P["tauA"], fs)
    GA = (1 + EA / P["EA0"]) ** (-P["pA"])
    GB = onepole((1 + (l / P["LB0"]) ** 2) ** (-P["pB"] / 2), P["tauB"], fs)
    CS = P["C0"] * GA * GB
    n = sosfilt(sos(3, 'high', F, fs), CS * l)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if parts:
        return w, dict(CS=CS, GA=GA, GB=GB, l=l, b=b)
    return w


def generated5(x, F, P, fs=FS, parts=False):
    """generated4 with free shapes: slow stage detects LP3(cA F) x; the instantaneous stage uses the patent's rational
    law of u = (l / LB0)^2: (1 + b1 u + b2 u^2) / (1 + a1 u + a2 u^2), smoothed by a one-pole with a direct share mB."""
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    dA = sosfilt(sos(3, 'low', F * P["cA"], fs), x)
    EA = onepole(np.abs(dA), P["tauA"], fs)
    GA = (1 + EA / P["EA0"]) ** (-P["pA"])
    u = (l / P["LB0"]) ** 2
    ci = (1 + P["b1"] * u + P["b2"] * u * u) / (1 + P["a1"] * u + P["a2"] * u * u)
    GB = (1 - P["mB"]) * onepole(ci, P["tauB"], fs) + P["mB"] * ci
    CS = P["C0"] * GA * GB
    n = sosfilt(sos(3, 'high', F, fs), CS * l)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if parts:
        return w, dict(CS=CS, GA=GA, GB=GB, l=l, b=b)
    return w


def generated6(x, F, P, fs=FS, parts=False):
    """generated5 plus a gain driven by the high-passed band u = HP3(F) l (the patent's detector after the output
    high-pass): GC = (1 + EC / EC0)^-pC, EC = one-pole(|u|, tauC)."""
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    u = sosfilt(sos(3, 'high', F, fs), l)
    dA = sosfilt(sos(3, 'low', F * P["cA"], fs), x)
    EA = onepole(np.abs(dA), P["tauA"], fs)
    GA = (1 + EA / P["EA0"]) ** (-P["pA"])
    uu = (l / P["LB0"]) ** 2
    ci = (1 + P["b1"] * uu + P["b2"] * uu * uu) / (1 + P["a1"] * uu + P["a2"] * uu * uu)
    GB = (1 - P["mB"]) * onepole(ci, P["tauB"], fs) + P["mB"] * ci
    EC = onepole(np.abs(u), P["tauC"], fs)
    GC = (1 + EC / P["EC0"]) ** (-P["pC"])
    CS = P["C0"] * GA * GB * GC
    n = sosfilt(sos(3, 'high', F, fs), CS * l)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if parts:
        return w, dict(CS=CS, GA=GA, GB=GB, GC=GC, l=l, b=b)
    return w


def generated7(x, F, P, fs=FS, parts=False):
    """Gain from a level detector with a second-order, asymmetric smoother: two attack / release followers in cascade
    on |LP3(cA F) x|; CS = C0 (1 + E/E0)^-p applied to l ('pre') or to HP3(F) l ('post')."""
    x = np.asarray(x, np.float64)
    b = sosfilt(sos(3, 'low', F, fs), x)
    l = sosfilt(sos(3, 'low', F / 3, fs), x)
    if "QA" in P:     # detector band: LP2(cA F, QA) * LP1(cB F) (Butterworth when QA = 1 and cB = cA)
        d = lp2(sosfilt(sos(1, 'low', F * P["cB"], fs), x), F * P["cA"], P["QA"], fs)
    else:
        d = sosfilt(sos(3, 'low', F * P["cA"], fs), x)
    kr = P.get("kr", 1.0)
    s1 = (80.0 / F) ** P.get("s1", 0.0); s2 = (80.0 / F) ** P.get("s2", 0.0)     # time constants vs Freq
    E1 = attrel(np.abs(d) ** kr, P["ta1"] * s1, P["tr1"] * s1, fs)
    E = ((1 - P.get("m1", 0.0)) * attrel(E1, P["ta2"] * s2, P["tr2"] * s2, fs) + P.get("m1", 0.0) * E1) ** (1.0 / kr)
    if "kB" in P:      # a second, fast level path (attack / release) on the same band; the larger level wins
        sB = (80.0 / F) ** P.get("sB", 0.0)
        EB = attrel(attrel(np.abs(d), P["taB"] * sB, P["trB"] * sB, fs), P["taB2"] * sB, P["trB2"] * sB, fs)
        E = np.maximum(E, P["kB"] * EB)
    CS = P["C0"] * (1 + E / P["E0"]) ** (-P["p"])
    pl = P.get("place", "post")
    if pl == "pre":
        n = sosfilt(sos(3, 'high', F, fs), CS * l)
    elif pl == "mid1":
        n = sosfilt(sec('high', 2, F, fs), CS * sosfilt(sec('high', 1, F, fs), l))
    elif pl == "mid2":
        n = sosfilt(sec('high', 1, F, fs), CS * sosfilt(sec('high', 2, F, fs), l))
    elif pl == "split":           # CS^a ahead of the first HP3, CS^(1-a) after it
        a = P["alpha"]; C = CS / P["C0"]
        n = P["C0"] * C ** (1 - a) * sosfilt(sos(3, 'high', F, fs), C ** a * l)
    else:
        n = CS * sosfilt(sos(3, 'high', F, fs), l)
    beta = P.get("beta", 1.0)
    q = sosfilt(sos(3, 'high', F / 3, fs), n * CS * (CS / P["C0"]) ** (beta - 1.0) * b)
    w = sosfilt(sos(3, 'high', F, fs), -n + P["k2"] * q)
    if parts:
        return w, dict(CS=CS, E=E, l=l, b=b)
    return w


def limit(gen, P, fs=FS):
    """generated-signal limiter of the reference: the gain drops at once to what each sample needs to stay under limT,
    then recovers as a one-pole (limRel) in the gain domain"""
    need = np.minimum(1.0, P.get("limT", 0.70287) / np.maximum(np.abs(gen), 1e-12))
    return gen * -attrel(-need, 1e-7, P.get("limRel", 0.02844), fs, y0=-1.0)
