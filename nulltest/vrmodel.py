"""ctypes front end of vrmodel.c (WVox Rider's riding model): ride(x, settings, params) -> gain in dB per sample."""
import os, ctypes
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.CDLL(os.environ.get('VRMODEL_DLL', os.path.join(HERE, 'vrmodel.dll')))
_lib.ride.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_double, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
NAMES = ['c', 'bias', 'fatt_s', 'fatt_f', 'frel', 'fhold', 'fpow', 'vatt', 'vrel', 'vk', 'ratt_s', 'ratt_f', 'rrel',
         'rpow', 'park', 'phold_s', 'phold_f', 'spill', 'v0']
# the reference's hidden ParamXML / preset values (Attack sets the _s / _f pairs) and the Phase 3 measurements; the
# follower's coefficients are squared (fpow 2), the ride's are not
DEFAULTS = dict(c=0.0, bias=-42.0, fatt_s=50.0, fatt_f=10.0, frel=1009.0, fhold=5000.0, fpow=2.0, vatt=0.01, vrel=0.01,
                vk=14.0, ratt_s=0.55, ratt_f=0.35, rrel=1000.0, rpow=1.0, park=1205.0, phold_s=500.0, phold_f=50.0,
                spill=0.0, v0=1.0)
SET_DEFAULT = dict(target=-21.0, rmin=-6.0, rmax=6.0, idle=0.0, vs=0.0, spill=-48.0, fast=0.0, live=0.0)


def ride(x, settings=None, params=None, fs=48000.0):
    s = dict(SET_DEFAULT, **(settings or {}))
    p = dict(DEFAULTS, **(params or {}))
    x = np.ascontiguousarray(x, np.float32)
    st = np.array([s['target'], s['rmin'], s['rmax'], s['idle'], s['vs'], s['spill'], s['fast'], s['live']], np.float64)
    pv = np.array([p[k] for k in NAMES], np.float64)
    out = np.zeros(len(x), np.float32)
    _lib.ride(x.ctypes.data, len(x), fs, st.ctypes.data, pv.ctypes.data, out.ctypes.data)
    return out
