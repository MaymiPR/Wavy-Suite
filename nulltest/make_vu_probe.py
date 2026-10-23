"""WAV for filming H-Comp's VU needle in FL: a 1 s count-in click, then steady 300 Hz tones (constant gain reduction),
bursts of 5 / 50 / 300 ms, the drum loop and the voice-like signal, with silences between. Writes
build/meters/vu_probe.wav (48 kHz stereo float) and its event times (vu_probe.json)."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wcomp as N

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'build', 'meters')
S = N.signals(FS)
t = lambda s: np.arange(int(s * FS)) / FS
parts, events, pos = [], [], 0.0
def add(name, x):
    global pos
    parts.append(np.asarray(x, np.float64)); events.append((name, pos, pos + len(x) / FS)); pos += len(x) / FS
add('click', np.r_[np.ones(48) * 0.9, np.zeros(FS - 48)])
add('silence', np.zeros(FS))
for db in (-6.0, -12.0, -26.0, -6.0):
    add('tone %g dBFS' % db, 10 ** (db / 20) * np.sin(2 * np.pi * 300 * t(2.0))); add('silence', np.zeros(int(1.5 * FS)))
for ms in (5, 50, 300):
    for k in range(3):
        add('burst %d ms' % ms, 10 ** (-6 / 20) * np.sin(2 * np.pi * 300 * t(ms / 1000))); add('silence', np.zeros(int(1.2 * FS)))
add('drum loop', S['drum loop'][:4 * FS] * 1.0); add('silence', np.zeros(FS))
add('voice-like', S['voice-like'][:4 * FS] * 1.0); add('silence', np.zeros(FS))
x = np.concatenate(parts).astype(np.float32)
os.makedirs(OUT, exist_ok=True)
import wave, struct
pcm = (np.clip(x, -1, 1) * 32767).astype('<i2')
with wave.open(os.path.join(OUT, 'vu_probe.wav'), 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(FS); w.writeframes(np.stack([pcm, pcm], 1).tobytes())
json.dump(events, open(os.path.join(OUT, 'vu_probe.json'), 'w'), indent=1)
print('vu_probe.wav %.1f s' % (len(x) / FS))
