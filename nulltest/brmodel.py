"""Python side of brmodel.c (WBass Rider's riding model, Phase 4): the parameters, the host controls' mapping, and a
run that returns the ride in dB lined up with the input (as the reference's output is after the host's latency
compensation: gain[t] multiplies x[t]).

    import brmodel as B;  g = B.ride(x, artifacts=50, sensitivity=50, response='Slow')
"""
import os, ctypes
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.CDLL(os.path.join(HERE, 'brmodel.dll'))
_lib.br_run.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_double, ctypes.POINTER(ctypes.c_double),
                        ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float)]
NAMES = ['target', 'rmin', 'rmax', 'idle', 'c0', 'sh_fc', 'sh_g', 'sh_q', 'pb_att', 'pb_rel', 'lp_fc', 'lp_q', 'n_att', 'n_rel',
         'floor', 'off', 'art', 'rel_depth', 'rel_ms', 'glide', 'smooth', 'ref_att', 'ref_rel', 'hp_fc', 'illegal', 'on_rise',
         'slow_att', 'slow_rel', 'mode', 'slow_rel_fast', 'thr_u', 'thr_a', 'hyst', 'settle', 'off_ms', 'win_ms', 'off_rel_ms', 'nmode', 'pitch_c', 'hold_min', 'win_latch', 'fast', 'block_rise', 'f_hi', 'f_lo', 'gate_block', 'att_mode', 'f_att', 'f_rel', 'band_hyst']
assert len(NAMES) == _lib.br_nparams()
DEFAULTS = dict(target=-14.0, rmin=-12.0, rmax=12.0, idle=0.0, c0=0.68, sh_fc=31.35, sh_g=-28.90, sh_q=0.5075, pb_att=2.0,
                pb_rel=100.0, lp_fc=1020.0, lp_q=0.60, n_att=1.0, n_rel=1.0, floor=-48.75, off=38.0, art=11.5, rel_depth=12.4,
                rel_ms=21.0, glide=32.0, smooth=5.0, ref_att=1.0, ref_rel=5000.0, hp_fc=2000.0, illegal=0.0, on_rise=0.3,
                slow_att=30.0, slow_rel=1000.0, mode=0.0, slow_rel_fast=0.0, thr_u=1.25, thr_a=0.05, hyst=0.3, settle=20.0, off_ms=60.0, win_ms=75.0, off_rel_ms=60.0, nmode=1.0, pitch_c=3.3, hold_min=0.0, win_latch=0.0, fast=0.0, block_rise=1.0, f_hi=172.0, f_lo=78.0, gate_block=1.0, att_mode=1.0, f_att=2.0, f_rel=100.0, band_hyst=0.0)
FS = 48000
LOOKAHEAD = {False: 2016, True: 240}      # samples at 48 kHz: 42 ms, Live 5 ms


def release_depth(response, sensitivity):
    """the dip a note needs before a re-attack is a new note (measured: Slow 12.5 dB at Sensitivity 0 .. 50, 1.5 at 75,
    under 1 at 100; Fast 4.5 at 0, 1.5 at 50, under 1 at 100); linear between the measured points for now"""
    pts = {'Slow': [(0, 12.5), (50, 12.5), (75, 1.5), (100, 0.5)], 'Fast': [(0, 4.5), (50, 1.5), (100, 0.5)]}[response.strip()]
    return float(np.interp(sensitivity, [a for a, _ in pts], [b for _, b in pts]))


def params(artifacts=50.0, sensitivity=50.0, response='Slow', target=-14.0, range_min=-12.0, range_max=12.0, idle=0.0, **raw):
    p = dict(DEFAULTS)
    p.update(target=target, rmin=range_min, rmax=range_max, idle=idle, art=20.0 - 0.17 * artifacts,
             rel_depth=release_depth(response, sensitivity) - 0.1)
    p.update(raw)
    return p


def run(x, p, trace=False):
    x = np.ascontiguousarray(np.asarray(x, np.float32))
    v = (ctypes.c_double * len(NAMES))(*[float(p[k]) for k in NAMES])
    g = np.zeros(len(x), np.float32)
    tr = np.zeros((len(x), 6), np.float32) if trace else None
    _lib.br_run(x.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), len(x), float(FS), v, g.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                tr.ctypes.data_as(ctypes.POINTER(ctypes.c_float)) if trace else None)
    return (g, tr) if trace else g


def ride(x, live=False, trace=False, **kw):
    """the ride in dB lined up with x (gain[t] meets x[t]); x mono (the mid for stereo)"""
    L = LOOKAHEAD[live]
    xp = np.r_[np.asarray(x, np.float32), np.zeros(L, np.float32)]
    r = run(xp, params(**kw), trace)
    g, tr = r if trace else (r, None)
    return (g[L:], tr[L:]) if trace else g[L:]
