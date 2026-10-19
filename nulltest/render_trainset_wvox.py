"""Training / validation renders for the learned WVox slow path (nulltest/train_wvox.py).

Each example: a 2 s mono signal at a random Compression (gate off, Gain 0), the reference's output, and the WVox
internals for the same input from tools/WVoxRender (dump=1): detector level v, fast ratio, analytical slow ratio, per
processed sample (index i + 64 acts on input i). Writes measurements/rvox/train/<split>_<n>.npz (git-ignored).

    python -I render_trainset_wvox.py <split> <count> <seed>
"""
import sys, os, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.signal import butter, sosfilt
import refhost as R
import nulltest_wvox as N

FS = 48000
SECS = 2.0
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "measurements", "rvox", "train")
os.makedirs(OUT, exist_ok=True)


def db(v): return 10 ** (v / 20)


def env_syll(rng, n):
    """syllable-like envelope: bursts with attack / decay and gaps"""
    e = np.zeros(n); pos = int(rng.uniform(0, 0.1) * FS)
    while pos < n:
        on = int(rng.uniform(0.05, 0.5) * FS); gap = int(rng.uniform(0.0, 0.3) * FS)
        t = np.arange(on) / FS
        att = rng.uniform(0.002, 0.06); rel = rng.uniform(0.02, 0.3)
        seg = np.minimum(1, t / att) * np.minimum(1, (on / FS - t) / rel) * db(rng.uniform(-24, 0))
        m = min(on, n - pos); e[pos:pos + m] = np.maximum(seg[:m], 0); pos += on + gap
    return e


def voice(rng, n):
    t = np.arange(n) / FS
    f0 = rng.uniform(80, 400) * (1 + 0.02 * np.sin(2 * np.pi * rng.uniform(4, 7) * t))
    ph = 2 * np.pi * np.cumsum(f0) / FS
    tilt = rng.uniform(0.7, 1.5)
    x = sum(np.sin(k * ph) / k ** tilt for k in range(1, 25) if k * f0.max() < 16000)
    if rng.random() < 0.5:                                    # a formant
        x = sosfilt(butter(2, sorted([rng.uniform(300, 900), rng.uniform(1200, 3500)]), btype='band', fs=FS, output='sos'), x) * 3
    return x * env_syll(rng, n)


def make(rng):
    n = int(SECS * FS); t = np.arange(n) / FS
    kinds = os.environ.get("KINDS", "voice,speechnoise,noise,tone,steps,am,sweep,drums,mix,lowtone").split(",")
    kind = str(rng.choice(kinds))
    if kind == "voice":
        x = voice(rng, n)
    elif kind == "speechnoise":
        z = rng.standard_normal(n)
        z = sosfilt(butter(2, sorted([rng.uniform(200, 1500), rng.uniform(2000, 8000)]), btype='band', fs=FS, output='sos'), z)
        x = z * env_syll(rng, n)
    elif kind == "noise":
        z = rng.standard_normal(n)
        if rng.random() < 0.5:
            Z = np.fft.rfft(z); fr = np.fft.rfftfreq(n, 1 / FS); Z[1:] /= fr[1:] ** rng.uniform(0, 1); Z[0] = 0; z = np.fft.irfft(Z, n)
        x = z / np.std(z) * (env_syll(rng, n) if rng.random() < 0.4 else 1.0)
    elif kind == "tone":
        x = np.sin(2 * np.pi * 10 ** rng.uniform(np.log10(60), np.log10(10000)) * t + rng.uniform(0, 6.3))
    elif kind == "lowtone":
        x = np.sin(2 * np.pi * rng.uniform(25, 250) * t + rng.uniform(0, 6.3))
    elif kind == "steps":
        f = 10 ** rng.uniform(np.log10(50), np.log10(5000)); x = np.zeros(n); pos = 0
        while pos < n:
            m = int(rng.uniform(0.03, 0.6) * FS); x[pos:pos + m] = db(rng.uniform(-60, 0)) * (rng.random() > 0.15); pos += m
        x = x * np.sin(2 * np.pi * f * t)
    elif kind == "am":
        f = 10 ** rng.uniform(np.log10(60), np.log10(8000)); fm = 10 ** rng.uniform(-0.5, 1.6)
        x = (1 + rng.uniform(0.2, 0.95) * np.sin(2 * np.pi * fm * t)) * np.sin(2 * np.pi * f * t)
    elif kind == "sweep":
        f0, f1 = 10 ** rng.uniform(1.4, 2.5), 10 ** rng.uniform(3, 4.2)
        x = np.sin(2 * np.pi * f0 * SECS / np.log(f1 / f0) * (np.exp(t / SECS * np.log(f1 / f0)) - 1))
    elif kind == "drums":
        x = np.zeros(n); pos = int(rng.uniform(0, 0.1) * FS)
        while pos < n:
            m = int(0.3 * FS); tt = np.arange(m) / FS
            hit = rng.standard_normal(m) * np.exp(-tt / rng.uniform(0.01, 0.15))
            if rng.random() < 0.5: hit += np.sin(2 * np.pi * rng.uniform(50, 120) * tt) * np.exp(-tt / rng.uniform(0.05, 0.3)) * 2
            seg = hit[:max(0, min(m, n - pos))] * db(rng.uniform(-20, 0)); x[pos:pos + len(seg)] += seg
            pos += int(rng.uniform(0.1, 0.6) * FS)
    else:  # mix: voice over noise or a bed
        bed = rng.standard_normal(n); bed = sosfilt(butter(1, rng.uniform(300, 6000), fs=FS, output='sos'), bed)
        x = voice(rng, n) / 3 + bed / np.std(bed) * db(rng.uniform(-40, -10))
    x = x / max(1e-9, np.max(np.abs(x))) * db(rng.uniform(float(os.environ.get("LOMIN", "-36")), -0.5))   # peak level
    return kind, x.astype(np.float32)


def internals(x, comp):
    fi, fo = os.path.join(N.TMP, "tin%d.f32" % os.getpid()), os.path.join(N.TMP, "tout%d.f32" % os.getpid())
    x.astype(np.float32).tofile(fi)
    subprocess.run([N.EXE, fi, fo, "1", str(FS), str(comp), "-80", "0", "dump=1"], check=True, capture_output=True)
    return np.fromfile(fo, np.float32), np.fromfile(fo + ".trace", np.float32).reshape(-1, 4)


if __name__ == "__main__":
    split, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    rng = np.random.default_rng(seed)
    for i in range(count):
        kind, x = make(rng)
        comp = float(np.round(-36 * rng.random() ** 0.7, 1))         # weighted toward heavy compression
        ref = R.render(x, FS, comp=comp)[0].astype(np.float32)
        ours, tr = internals(x, comp)
        np.savez_compressed(os.path.join(OUT, "%s_%04d.npz" % (split, i)), x=x, ref=ref, ours=ours, v=tr[:, 0], fast=tr[:, 1],
                            slow=tr[:, 2], comp=comp, kind=kind)
        if i % 25 == 0:
            print(split, i, kind, comp, "null %.1f dB" % N.residual_db(ours, ref, int(0.2 * FS)), flush=True)
