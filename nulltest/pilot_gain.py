"""The compressor's gain over time from a MeterCapture run of vu_probe_pilot.f32: the -54 dBFS 6170 Hz pilot (no harmonic of the 300 Hz tones),
demodulated from input and output (64-sample latency removed), low-passed at 1 kHz; their ratio over the makeup is the
gain, also through the probe's silences. Writes <capture>_g.npy (t, g at 1 ms steps). Private measurement helper.

    python -I pilot_gain.py <capture.npz> <makeup dB>
"""
import sys
import numpy as np

FS, F0, LAT = 48000, 6170.0, 64
fn, mk = sys.argv[1], float(sys.argv[2])
x = np.fromfile(fn.replace('.npz', '').rsplit('mc_', 1)[0] + 'vu_probe_pilot.f32', np.float32).astype(np.float64)
y = np.load(fn)['audio'][0].astype(np.float64)
n = min(len(x), len(y) - LAT); x, y = x[:n], y[LAT:LAT + n]
ph = np.exp(-2j * np.pi * F0 * np.arange(n) / FS)
def lp(v):                                           # 1 kHz brick wall, zero phase
    V = np.fft.rfft(v); f = np.fft.rfftfreq(len(v), 1 / FS); V[f > 1000] = 0; return np.fft.irfft(V, len(v))
dx = lp((x * ph).real) + 1j * lp((x * ph).imag); dy = lp((y * ph).real) + 1j * lp((y * ph).imag)
g = np.abs(dy) / np.maximum(np.abs(dx), 1e-12) / 10 ** (mk / 20)
np.save(fn.replace('.npz', '_g.npy'), np.stack([np.arange(n)[::48] / FS, g[::48]]))
print(fn, 'g %.3f..%.3f, at 1.5 s %.4f' % (g.min(), g.max(), g[int(1.5 * FS)]))
