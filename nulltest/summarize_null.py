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
        '- **Steady tones match to numerical precision** (about -95 to -120 dB at 1 kHz and 8 kHz, at 44.1 to 192 kHz, mono and stereo). A null is claimed only for these stationary cases and for Compression 0 (a fixed gain).',
        '- **Level changes are close but not a null**: steps and sweeps about -35 to -45 dB, a 100 Hz tone about -40 dB, amplitude modulation about -25 dB, noise bursts about -27 dB, white / pink noise about -20 dB, voice-like material about -21 dB. The detector, look-ahead and gate are identified exactly (see MEASUREMENTS.md, "Detector model"); the slow path of the compressor is a fitted model, so on dynamic material expect a small difference in how the gain moves, not in the tone.',
        '- The gate matches to 0.0004 dB rms on level steps; with gate and compression together the residual follows the compressor numbers.',
        '- WVox was tuned on the same kinds of signals (steps, bursts, AM, noise, voice-like, sweeps); these numbers are therefore not an independent test set.']
open(P, 'w', encoding='utf-8').write(txt + '\n'.join(out) + '\n')
print('ok')
