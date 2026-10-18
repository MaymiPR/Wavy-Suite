"""Input for tools/MeterCapture to identify the reference's meter law: level steps while a tone keeps playing, tones of
different frequencies, and short bursts. Writes build/meters/probe.f32 (mono float32, 48 kHz) and probe.json.

    python -I meter_probe.py
"""
import os, json
import numpy as np

FS = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "meters")
SEGS = [(1.0, None, 0), (1.5, -6, 50), (1.5, -12, 50), (2.0, None, 0), (1.5, -6, 300), (2.0, None, 0),
        (1.5, -6, 25), (2.0, None, 0), (0.005, -6, 300), (1.5, None, 0), (0.02, -6, 300), (1.5, None, 0),
        (0.1, -6, 300), (1.5, None, 0), (1.0, -6, 50), (1.5, -26, 50), (2.0, None, 0), (1.5, -12, 1000), (2.0, None, 0)]

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    x, ph = [], 0.0
    for secs, lvl, f in SEGS:
        n = int(round(secs * FS))
        if lvl is None:
            x.append(np.zeros(n)); continue
        x.append(10 ** (lvl / 20) * np.sin(ph + 2 * np.pi * f * np.arange(n) / FS))
        ph += 2 * np.pi * f * n / FS
    x = np.concatenate(x).astype(np.float32)
    x.tofile(os.path.join(OUT, "probe.f32"))
    json.dump(SEGS, open(os.path.join(OUT, "probe.json"), "w"))
    print("probe %.1f s" % (len(x) / FS))
