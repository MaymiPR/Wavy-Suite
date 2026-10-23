"""Calibration probe for H-Comp's In / Out needle (filmed with MeterCapture gl=1): 1 s of silence, a 300 Hz sine
staircase from 0 to -60 dBFS (peak) in 3 dB steps of 1 s, then 1 s steps alternating a sine and a square of the same
peak at -6, -12, -18 and -24 dBFS (a peak reading shows them alike, an RMS one 3 dB apart). Writes
build/meters/vu_cal.f32 (mono float32, 48 kHz) and vu_cal.json (the steps)."""
import os, json
import numpy as np

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'meters')
t = np.arange(FS) / FS
parts, steps, pos = [np.zeros(FS)], [], 1.0
def add(kind, db):
    global pos
    s = np.sin(2 * np.pi * 300 * t)
    parts.append(10 ** (db / 20) * (s if kind == 'sine' else np.sign(s))); steps.append((kind, db, pos, pos + 1.0)); pos += 1.0
for db in range(0, -61, -3): add('sine', float(db))
for db in (-6.0, -12.0, -18.0, -24.0): add('sine', db); add('square', db)
x = np.concatenate(parts).astype(np.float32)
x.tofile(os.path.join(OUT, 'vu_cal.f32'))
json.dump(steps, open(os.path.join(OUT, 'vu_cal.json'), 'w'), indent=1)
print('vu_cal.f32 %.1f s' % (len(x) / FS))
