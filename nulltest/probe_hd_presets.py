"""Probe: the weak presets with one thing changed at a time (Analog Off, Depth 0, Feedback 0, ...), to find the part that
does not null.

    python -I probe_hd_presets.py "Preset Name" ...
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import nulltest_wdelay as N
import nulltest_wdelay_presets as P

VARIANTS = [("as stored", {}), ("Analog Off", dict(analog=0)), ("Depth 0", dict(depth=0)), ("Feedback 0", dict(feedback=0)),
            ("Analog Off, Depth 0", dict(analog=0, depth=0)), ("Analog Off, Feedback 0", dict(analog=0, feedback=0)),
            ("Analog Off, LoFi off", dict(analog=0, lofi=0)), ("Analog Off, filters off", dict(analog=0, hipass=19, lopass=20138.77))]


def main():
    fs = 48000
    S = N.signals(fs)
    x = S[[k for k in S if 'voice' in k][0]]
    allp = {n: v for g, n, v in P.presets()}
    for name in sys.argv[1:]:
        print(name, allp[name])
        for vn, ch in VARIANTS:
            v = dict(allp[name]); v.update(ch)
            o, r = P.render_pair(np.stack([x, 0.8 * x]), fs, v)
            np.save(os.path.join(N.TMP, 'probe_%s_%s.npy' % (name.replace(' ', '_'), vn.replace(' ', '_').replace(',', ''))), np.stack([o, r]))
            print('  %-26s %6.1f dB' % (vn, N.residual_db(o, r)), flush=True)


if __name__ == '__main__':
    main()
