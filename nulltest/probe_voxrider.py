"""Phase 1 probe of the reference Vocal Rider (WaveShell VST3, the user's install): the components the shell offers,
each one's VST3 parameters (name, default text, the values a host can set), the reported latency, the real delay of
the audio (cross-correlation in and out), the input buses a host sees (tools/RefRender) and the gain it applies to a
steady voice-like buzz at a few levels (the default Target -21).

    python -I probe_voxrider.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import subprocess, tempfile
import numpy as np
import pedalboard as pb
import refhost as R


def buzz(t, f0=180.0, n=20):
    """A voice-like buzz: harmonics of f0 falling 6 dB / octave, normalised to unit rms."""
    x = sum(np.sin(2 * np.pi * f0 * k * t) / k for k in range(1, n + 1))
    return x / np.sqrt(np.mean(x ** 2))


def main():
    names = pb.VST3Plugin.get_plugin_names_for_file(R.WAVESHELL)
    vr = [n for n in names if "Vocal Rider" in n]
    print("shell components with 'Vocal Rider':", vr)
    for name in vr:
        R.fresh(name); p = R.load(name)
        print("\n==", name, "latency (reported)", getattr(p, "reported_latency_samples", "?"))
        for k, q in p.parameters.items():
            if q.string_value == "": continue                 # the shell's MIDI controller slots
            vals = getattr(q, "valid_values", None)
            vs = (" values %s" % (vals[:6] + (["..."] if len(vals) > 6 else []) if len(vals) <= 400 else "%d values" % len(vals))) if vals else ""
            print("  %-22s %-14s %s%s" % (k, repr(q.string_value), getattr(q, "label", ""), vs))
    fs = 48000
    rng = np.random.default_rng(3)
    t = np.arange(int(6 * fs)) / fs
    # syllables: 0.2 .. 0.4 s buzzes at random pitches and levels with 0.1 s gaps
    x = np.zeros_like(t); s = 0
    while s < len(t) - fs // 2:
        n = int(rng.uniform(0.2, 0.4) * fs); a = 10 ** (rng.uniform(-36, -9) / 20)
        tt = np.arange(n) / fs
        x[s:s + n] += a * buzz(tt, rng.uniform(120, 300)) * np.abs(np.sin(np.pi * tt / tt[-1])) ** 0.5
        s += n + int(0.1 * fs)
    x = x.astype(np.float32)
    for name in vr:
        R.fresh(name); p = R.load(name)
        xin = x[None] if "Mono" in name else np.stack([x, x])
        y = p(xin, fs, buffer_size=512)[0]
        c = np.correlate(y[:int(4 * fs)], x[:int(4 * fs)], 'full')[len(x[:int(4 * fs)]) - 1:]
        lag = int(np.argmax(np.abs(c[:int(0.2 * fs)])))
        print("%-26s real delay %d samples (%.2f ms) at %d Hz" % (name, lag, lag / fs * 1000, fs))
    # the input buses (a short render through RefRender, which enables any side-chain bus)
    d = tempfile.mkdtemp()
    xi = np.zeros((2, fs // 10), np.float32); xi.T.tofile(os.path.join(d, "in.f32"))
    for name in vr:
        uid = {"Vocal Rider Mono": -510562340, "Vocal Rider Stereo": -509086244,
               "Vocal Rider Live Mono": 989902242, "Vocal Rider Live Stereo": 991378338}[name]
        out = subprocess.run([R.REFRENDER, R.WAVESHELL, "%s@%d" % (name, uid), os.path.join(d, "in.f32"),
                              os.path.join(d, "out.f32")], capture_output=True, text=True).stdout
        print(name, "\n" + "\n".join(l for l in out.splitlines() if "bus" in l))
    # the gain on steady buzzes (after 4 s), at the default settings
    name = "Vocal Rider Stereo"
    for lev in (-48, -36, -27, -21, -15, -9, -3):
        R.fresh(name); p = R.load(name)
        xs = (10 ** (lev / 20) * buzz(t)).astype(np.float32)
        y = p(np.stack([xs, xs]), fs, buffer_size=512)[0]
        seg = slice(int(4.5 * fs), int(5.5 * fs))
        g = 20 * np.log10(np.sqrt(np.mean(y[seg] ** 2)) / np.sqrt(np.mean(xs[seg] ** 2)))
        print("buzz at %+d dBFS rms: gain %+.2f dB (ride %s)" % (lev, g, p.parameters['ride'].string_value if 'ride' in p.parameters else '?'))


if __name__ == "__main__":
    main()
