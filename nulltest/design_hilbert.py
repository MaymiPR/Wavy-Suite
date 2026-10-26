"""Designs the quadrature allpass pair WBass Rider's envelopes use: two chains of second-order allpass sections in z^-2
(y = c (x + y[n-2]) - x[n-2], c = a^2), the second chain with one sample more delay, so the two outputs are 90 degrees
apart over the band; |a + j b| is then the analytic envelope with no look-ahead (the reference's ramp starts exactly at
its 42 ms look-ahead, so its envelope adds no delay of its own). Optimises the coefficients for the largest band
error over 12 Hz .. 20 kHz at 48 kHz and prints them for the C++ (and nulltest/brmodel.c).

    python -I design_hilbert.py [sections per chain, default 6] [sample rate, default 48000]
"""
import sys
import numpy as np
from scipy import optimize

K = int(sys.argv[1]) if len(sys.argv) > 1 else 6
FS = float(sys.argv[2]) if len(sys.argv) > 2 else 48000.0
f = np.geomspace(12.0, min(20000.0, 0.45 * FS), 600)
z1 = np.exp(-1j * 2 * np.pi * f / FS)
z2 = z1 * z1


def chain(c):
    h = np.ones_like(z1)
    for ci in c: h *= (ci - z2) / (1 - ci * z2)
    return h


def phase_err(p):
    c = 1.0 / (1.0 + np.exp(-np.asarray(p)))                  # coefficients kept in (0, 1)
    ha, hb = chain(c[:K]), chain(c[K:]) * z1
    d = np.angle(ha / hb)
    return np.degrees(np.abs(np.abs(d) - np.pi / 2))


best = None
rng = np.random.default_rng(1)
for trial in range(24):
    # starting values: poles spread geometrically across the band, interleaved between the chains
    u = np.sort(rng.uniform(0.0, 1.0, 2 * K)) if trial else np.linspace(0.02, 0.98, 2 * K)
    c0 = 1.0 - np.geomspace(1e-4, 0.9, 2 * K)[::-1] ** (0.5 + 0.0 * u)
    c0 = np.clip(c0 + 0.02 * rng.standard_normal(2 * K) * (trial > 0), 1e-4, 1 - 1e-6)
    p0 = np.log(c0 / (1 - c0))
    p0 = np.r_[p0[0::2], p0[1::2]]
    r = optimize.minimize(lambda p: np.max(phase_err(p)), p0, method='Nelder-Mead', options=dict(maxiter=40000, maxfev=40000, xatol=1e-9, fatol=1e-9))
    r = optimize.minimize(lambda p: np.log(np.sum(np.exp(8 * phase_err(p))) ) / 8, r.x, method='BFGS')
    e = np.max(phase_err(r.x))
    if best is None or e < best[0]: best = (e, r.x)
    print('trial %2d: max error %.4f deg' % (trial, e), flush=True)
e, p = best
c = 1.0 / (1.0 + np.exp(-p))
print('best at %.0f Hz, %d + %d sections: max phase error %.4f deg over 12 Hz .. %.0f Hz (envelope ripple about +-%.3f dB)' % (FS, K, K, e, f[-1], 20 * np.log10(1 + np.sin(np.radians(e)))))
print('chain A:', ', '.join('%.10ff' % v for v in np.sort(c[:K])))
print('chain B:', ', '.join('%.10ff' % v for v in np.sort(c[K:])))
