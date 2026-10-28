"""Phase 1 probe of the reference CLA Vocals (WaveShell VST3, the user's install): the components the shell offers,
each one's VST3 parameters (name, default text, the values a host can set; pedalboard hosts only the Stereo one), and
the reported latency with the real delay of the audio (an impulse in, the output's first peak) at 44.1 / 48 / 96 kHz.

    python -I probe_voxchain.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import pedalboard as pb
import refhost as R

UIDS = ('CLA Vocals Stereo@1510537014', 'CLA Vocals Mono/Stereo@1511767094')


def main():
    names = pb.VST3Plugin.get_plugin_names_for_file(R.WAVESHELL)
    cv = [n for n in names if "CLA Vocals" in n]
    print("shell components with 'CLA Vocals':", cv)
    for name in cv:
        if "Mono" in name: continue                        # (pedalboard cannot host 1 in / 2 out: RefRender below)
        R.fresh(name); p = R.load(name)
        print("\n==", name, "latency (reported)", getattr(p, "reported_latency_samples", "?"))
        for k, q in p.parameters.items():
            if q.string_value == "": continue                 # the shell's MIDI controller slots
            vals = getattr(q, "valid_values", None)
            vs = (" values %s" % (vals[:8] + (["..."] if len(vals) > 8 else []) if len(vals) <= 400 else "%d values" % len(vals))) if vals else ""
            print("  %-22s %-14s %s%s" % (k, repr(q.string_value), getattr(q, "label", ""), vs))
    # the delay through RefRender, which hosts both components (a shell's sub-plugin by name@uid: by name alone it can
    # load another sub-plugin); it reports the latency and removes it
    for name in UIDS:
        for fs in (44100, 48000, 96000):
            n = int(1.5 * fs); x = np.zeros(n, np.float32); x[int(0.5 * fs)] = 0.5
            y, out = R.render_ref(x, fs, name=name)
            k = int(np.argmax(np.abs(y[0]))) - int(0.5 * fs)
            info = [l.strip() for l in out.splitlines() if 'latency' in l]
            print('%-34s %5d Hz: first peak %+d samples after the reported latency, peak L %.3f R %.3f | %s'
                  % (name, fs, k, np.abs(y[0]).max(), np.abs(y[1]).max(), ' / '.join(info)[:120]))

if __name__ == '__main__':
    main()
