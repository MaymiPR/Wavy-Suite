"""Training / validation renders for the learned WBass gain block (nulltest/train_gain.py).

Each example: a 2 s mono signal, a Freq setting and Decay 0 / 1, and the reference's exact generated part, split off
with two renders at Intensity -12 and -24 dB (both below the generated-signal limiter; Intensity is an exact gain
there). Original Bass out. Writes measurements/rbass/train/<split>_<n>.npz (git-ignored).

    python -I render_trainset.py <split> <count> <seed>
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, sosfilt
import refhost as R

FS = 48000
N = 2 * FS
FREQS = (32, 40, 50, 63, 80, 100, 125, 160, 200, 256)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rbass", "train")
os.makedirs(OUT, exist_ok=True)


def db(v): return 10 ** (v / 20)


def env_adsr(rng, n):
    e = np.zeros(n); pos = 0
    while pos < n:
        on = int(rng.uniform(0.05, 0.6) * FS); off = int(rng.uniform(0.0, 0.4) * FS)
        att = max(1, int(rng.uniform(0.0005, 0.03) * FS)); dec = rng.uniform(0.05, 1.5)
        t = np.arange(on) / FS
        seg = np.minimum(1, np.arange(on) / att) * np.exp(-t / dec) * db(rng.uniform(-30, 0))
        e[pos:pos + on] = seg[:max(0, min(on, n - pos))]
        pos += on + off
    return e


def make(rng, F):
    t = np.arange(N) / FS
    kinds = os.environ.get("KINDS", "tone,steps,am,twotone,noise,kick,bassline,pinned,mix,sweep").split(",")
    kind = rng.choice(kinds)
    lvl = rng.uniform(-55, -3)
    if kind == "tone":
        f = F * rng.uniform(0.08, 2.0)
        x = db(lvl) * np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28))
    elif kind == "steps":
        f = F * rng.uniform(0.1, 1.6); x = np.zeros(N); pos = 0
        while pos < N:
            m = int(rng.uniform(0.05, 0.5) * FS); x[pos:pos + m] = db(rng.uniform(-60, -3)) * (rng.random() > 0.2); pos += m
        x = x * np.sin(2 * np.pi * f * t)
    elif kind == "am":
        f = F * rng.uniform(0.1, 1.6); fm = 10 ** rng.uniform(-0.5, 1.8)
        x = db(lvl) * (1 + rng.uniform(0.1, 0.9) * np.sin(2 * np.pi * fm * t)) * np.sin(2 * np.pi * f * t)
    elif kind == "twotone":
        f1, f2 = F * rng.uniform(0.1, 1.5, 2)
        x = db(lvl) * np.sin(2 * np.pi * f1 * t) + db(lvl + rng.uniform(-20, 10)) * np.sin(2 * np.pi * f2 * t)
    elif kind == "noise":
        z = rng.standard_normal(N); hi = F * rng.uniform(0.5, 6)
        z = sosfilt(butter(2, min(hi, 20000), fs=FS, output='sos'), z)
        x = z / np.std(z) * db(lvl) * (env_adsr(rng, N) if rng.random() < 0.5 else 1)
    elif kind == "kick":
        x = np.zeros(N); pos = int(rng.uniform(0, 0.2) * FS)
        while pos < N:
            m = int(0.5 * FS); tt = np.arange(m) / FS
            f_end = rng.uniform(35, 70); f_start = f_end + rng.uniform(20, 150); tau = rng.uniform(0.01, 0.06)
            ph = 2 * np.pi * (f_end * tt + (f_start - f_end) * tau * (1 - np.exp(-tt / tau)))
            k = db(rng.uniform(-24, 0)) * np.exp(-tt / rng.uniform(0.05, 0.4)) * np.sin(ph)
            seg = k[:max(0, min(m, N - pos))]; x[pos:pos + len(seg)] += seg
            pos += int(rng.uniform(0.15, 0.7) * FS)
    elif kind == "bassline":
        x = np.zeros(N); pos = 0
        while pos < N:
            m = int(rng.uniform(0.1, 0.6) * FS); tt = np.arange(m) / FS; f = rng.uniform(30, 130)
            harm = sum(np.sin(2 * np.pi * f * h * tt) / h ** rng.uniform(1, 2.5) for h in range(1, 6))
            seg = db(rng.uniform(-30, -3)) * np.minimum(1, tt / 0.003) * np.exp(-tt / rng.uniform(0.1, 1.0)) * harm
            seg = seg[:max(0, min(m, N - pos))]; x[pos:pos + len(seg)] += seg; pos += m
    elif kind == "pinned":
        f0 = F * rng.uniform(0.1, 0.6); fb = F * rng.uniform(1.5, 3.5)
        x = db(lvl) * np.sin(2 * np.pi * f0 * t) + db(rng.uniform(-25, -3)) * np.sin(2 * np.pi * fb * t)
    elif kind == "mix":
        z = rng.standard_normal(N); z = sosfilt(butter(1, 300, fs=FS, output='sos'), z); z *= db(rng.uniform(-40, -15)) / np.std(z)
        f = rng.uniform(35, 110)
        x = z + db(lvl) * env_adsr(rng, N) / max(1e-9, db(-10)) * 0.3 * np.sin(2 * np.pi * f * t)
    else:  # sweep
        f0, f1 = F * rng.uniform(0.08, 0.5), F * rng.uniform(1, 4)
        x = db(lvl) * np.sin(2 * np.pi * f0 * 2.0 / np.log(f1 / f0) * (np.exp(t / 2.0 * np.log(f1 / f0)) - 1))
    x = np.clip(x, -0.99, 0.99)
    return str(kind), x.astype(np.float32)


if __name__ == "__main__":
    split, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    rng = np.random.default_rng(seed)
    for i in range(count):
        F = float(rng.choice(FREQS)); decay = 1 if rng.random() < 0.75 else 0
        kind, x = make(rng, F)
        a = R.render_rbass(x, FS, freq=F, intensity=-12, hp=1, extra={8: decay})[0].astype(np.float64)
        b = R.render_rbass(x, FS, freq=F, intensity=-24, hp=1, extra={8: decay})[0].astype(np.float64)
        gen = (a - b) / (db(-12) - db(-24))          # the generated part at Intensity 0
        np.savez_compressed(os.path.join(OUT, "%s_%04d.npz" % (split, i)), x=x, gen=gen.astype(np.float32), F=F, decay=decay, kind=kind)
        if i % 20 == 0: print(split, i, kind, F, decay, "peak gen %.3f" % np.max(np.abs(gen)), flush=True)
