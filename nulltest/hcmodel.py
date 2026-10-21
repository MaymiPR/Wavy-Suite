"""WComp Phase 4: a Python model of H-Comp's gain path, built up as it is identified (ident_hc_*.py)."""
import os, json
import numpy as np

_RES = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hcomp', 'results.json')))
KM = _RES['knee_model']


_SH = np.loadtxt(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hcomp', 'static_h.csv'), delimiter=',', skiprows=1)
_ST = {'x': _SH[:, 0], 'h': _SH[:, 1]}


def static_gain_db(A, thr, rat):
    """Static gain (dB, <= 0) of the compressor for a detector level A (linear peak), without makeup: GR = h(x),
    x = (1 - 1/ratio) * max(0, 20 log10(A + 0.001) - threshold), h the measured table (static_h.csv)."""
    x = (1 - 1 / rat) * np.maximum(0.0, 20 * np.log10(np.asarray(A) + 0.001) - thr)
    xs, hs = _ST['x'], _ST['h']
    gr = np.where(x >= xs[-1], x - (xs[-1] - hs[-1]), np.interp(x, xs, hs))
    return -np.where(x <= 0, 0.0, gr)


# ---- the per-sample gain path (hcfast.c) ---------------------------------------------------------------------
import ctypes as _ct
_dll = _ct.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hcfast.dll'))
P_NAMES = ['thr', 'ratio', 'eps', 'knee', 'toff', 'att_c', 'rel_c', 'fast_rate', 'slow_rate', 'charge_c', 'mode', 'D', 'H',
           'lead', 'B1', 'B2', 'fast_exp']


def params(**kw):
    d = dict(thr=-18.0, ratio=3.0, eps=KM['eps'], knee=KM['knee_db'], toff=KM['threshold_offset_db'], att_c=0.0, rel_c=0.0,
             fast_rate=0.0, slow_rate=0.0, charge_c=0.0, mode=0, D=0, H=64, lead=1, B1=38, B2=27, fast_exp=0.0)
    d.update(kw)
    return np.array([float(d[k]) for k in P_NAMES])


def gain(a, p, trace=False):
    """a: |x| per sample (float64). Returns the gain (linear, no makeup) [and the Ef, Es, gt traces]."""
    a = np.ascontiguousarray(a, np.float64)
    g = np.empty_like(a)
    tr = np.empty(3 * len(a)) if trace else None
    _dll.hc_gain(a.ctypes.data_as(_ct.c_void_p), g.ctypes.data_as(_ct.c_void_p), tr.ctypes.data_as(_ct.c_void_p) if trace else None,
                 len(a), np.ascontiguousarray(p, np.float64).ctypes.data_as(_ct.c_void_p))
    return (g, tr.reshape(-1, 3)) if trace else g
