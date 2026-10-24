"""Phase 1 probe of the reference H-Delay (WaveShell VST3, the user's install): the components the shell offers, each
one's VST3 parameters (name, default text, the values a host can set), its reported latency at 44.1 / 48 / 96 kHz,
and where an impulse comes out with everything at its defaults except Analog Off, Feedback 0, Sync ms.

    python -I probe_hdelay.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import pedalboard as pb
import refhost as R

COMPONENTS = ("H-Delay Mono", "H-Delay Stereo", "H-Delay Mono/Stereo", "H-Delay M>S")


def main():
    names = pb.VST3Plugin.get_plugin_names_for_file(R.WAVESHELL)
    hd = [n for n in names if "Delay" in n]
    print("shell components with 'Delay':", hd)
    for name in hd:
        try: R.fresh(name); p = R.load(name)
        except ValueError as e: print("\n==", name, "not loadable here:", e); continue   # mono in, stereo out
        print("\n==", name, "latency (reported)", getattr(p, "reported_latency_samples", "?"))
        for k, q in p.parameters.items():
            if q.string_value == "": continue                 # the shell's MIDI controller slots
            vals = getattr(q, "valid_values", None)
            vs = (" values %s" % (vals[:6] + (["..."] if len(vals) > 6 else []) if len(vals) <= 400 else "%d values" % len(vals))) if vals else ""
            print("  %-22s %-14s %s%s" % (k, repr(q.string_value), getattr(q, "label", ""), vs))
    # impulse at the defaults with no feedback, plain ms sync, Analog Off: where does the first echo land?
    for fs in (44100, 48000, 96000):
        R.fresh(hd[1]); p = R.load(hd[1])
        for k, v in (("sync", "ms "), ("feedback", 0.0), ("analog", "Off "), ("mix", 100.0)):
            if k in p.parameters:
                try: setattr(p, k, v)
                except Exception as e: print("  cannot set", k, v, e)
        x = np.zeros((2, int(fs * 1.5)), np.float32); x[:, int(0.2 * fs)] = 0.5
        y = p(x, fs, buffer_size=512)
        k = int(np.argmax(np.abs(y[0]))); d = (k - int(0.2 * fs)) / fs * 1000
        print("fs %d: largest output sample at %+.3f ms after the impulse (%d samples), value %.4f; delay text %s; latency %s" % (
            fs, d, k - int(0.2 * fs), y[0, k], p.parameters.get("delay", p.parameters.get("delay_time", None)).string_value
            if any(n in p.parameters for n in ("delay", "delay_time")) else "?", getattr(p, "reported_latency_samples", "?")))


if __name__ == "__main__" and "--echoes" not in sys.argv:
    main()


def echoes():
    """the first echoes of an impulse: Sync ms, Delay 100 ms, Analog Off, Feedback 60 / 0, Mix 100 / 50, 48 kHz"""
    fs = 48000
    for fb, mix, lofi in ((0.0, 100.0, "HiFi "), (60.0, 100.0, "HiFi "), (0.0, 50.0, "HiFi "), (0.0, 100.0, "LoFi ")):
        R.fresh("H-Delay Stereo"); p = R.load("H-Delay Stereo")
        p.sync = "ms "; p.analog = "Off "; p.feedback = fb; p.mix = mix; p.lofi = lofi
        p.delay_sec = 100.0
        x = np.zeros((2, fs), np.float32); x[:, int(0.1 * fs)] = 0.5
        y = p(x, fs, buffer_size=512)[0] / 0.5
        i0 = int(0.1 * fs)
        taps = [(k, y[i0 + k]) for k in range(-2, 3)]
        print("fb %3g mix %3g %s: delay text %s; at the impulse %.4f; echoes:" % (fb, mix, lofi.strip(), p.delay_sec.string_value if hasattr(p.delay_sec, "string_value") else p.parameters["delay_sec"].string_value, y[i0]))
        for e in range(1, 4):
            seg = y[i0 + e * 4800 - 40: i0 + e * 4800 + 200]
            k = int(np.argmax(np.abs(seg))) - 40
            print("   echo %d: peak %+.4f at %+d samples from %d ms, energy %.4f (%.2f dB), 4 taps around the peak %s" % (
                e, seg[k + 40], k, e * 100, float(np.sum(seg ** 2)), 10 * np.log10(max(float(np.sum(seg ** 2)), 1e-20)),
                np.round(seg[k + 38: k + 43], 4)))


if __name__ == "__main__" and "--echoes" in sys.argv:
    echoes()
