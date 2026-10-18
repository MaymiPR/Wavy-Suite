"""Adds a per-signal summary and the honest reading to docs/NULL_RESULTS.md (idempotent)."""
import os, re, collections, numpy as np
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'NULL_RESULTS.md')
txt = open(P, encoding='utf-8').read()
txt = txt.split('\n## Summary by signal')[0].rstrip() + '\n'
rows = [l.split('|')[1:6] for l in txt.splitlines() if l.startswith('| ') and not l.startswith('| signal') and not l.startswith('|---')]
by = collections.OrderedDict()
for sig, st, a, s, pk in rows:
    if not st.strip().startswith('0/'):                      # compression 0 only applies the fixed x0.99
        by.setdefault(sig.strip(), []).append((float(a), float(s)))
out = ['', '## Summary by signal', '',
       'Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.', '',
       '| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |', '|---|---:|---:|---:|---:|']
for sig, v in by.items():
    out.append('| %s | %.1f | %.1f | %.1f | %.1f |' % (sig, min(x[0] for x in v), np.median([x[0] for x in v]), max(x[0] for x in v), np.median([x[1] for x in v])))
out += ['', '## How to read this', '',
        '- **Steady tones at 1 kHz and 8 kHz match to numerical precision** (-75 dB and below; mostly -105 to -130 dB): the static law, the look-ahead, the true-peak level reading and the fixed x0.99 are right. A perfect null is claimed only for these stationary cases, and for Compression 0 (which is a fixed gain).',
        '- **Everything with level changes is only close, not a null**: steps and sweeps -30 to -45 dB, 100 Hz tone about -39 dB, amplitude modulation about -24 dB, noise and noise bursts about -17 to -22 dB, voice-like material about -18 dB. The reference\'s detector and release behaviour is only partly understood (see MEASUREMENTS.md, "Detector model"); WVox approximates it with a fitted model, so on dynamic programme material expect an audible difference in how the gain moves, not in the tone.',
        '- The gate is a hard-knee 2:1 expander on the same detector; with a gate and compression together the gain is within about 0.3 dB of the reference around the threshold (the -40 dB tone with a -40 dB gate nulls to only -64 dB).',
        '- WVox was tuned on the same kind of signals (steps, bursts, AM, noise, voice-like, sweeps); the numbers above are therefore not an independent test set.']
open(P, 'w', encoding='utf-8').write(txt + '\n'.join(out) + '\n')
print('ok')
