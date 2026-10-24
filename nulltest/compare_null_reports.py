"""Compares two null reports (NULL_RESULTS_WCOMP*.md tables): per case before / after, the cases that moved most.
    python -I compare_null_reports.py <before.md> <after.md> [presets]"""
import sys, statistics


def rows(fn, presets):
    out = {}
    for l in open(fn, encoding='utf-8'):
        c = [x.strip() for x in l.strip().strip('|').split('|')]
        if presets and len(c) == 10 and c[0] not in ('preset', '---'):
            for k, v in zip(('voice', 'drums', 'bass', 'noise'), c[1:5]):
                try: out[(c[0], k)] = float(v)
                except ValueError: pass
        elif not presets and len(c) == 11:
            try: out[tuple(c[:10])] = float(c[10])
            except ValueError: pass
    return out


presets = len(sys.argv) > 3
a, b = rows(sys.argv[1], presets), rows(sys.argv[2], presets)
keys = [k for k in a if k in b]
d = sorted(((b[k] - a[k], k) for k in keys), key=lambda q: q[0])
print('%d cases; better by >1 dB: %d, worse by >1 dB: %d; median %.1f -> %.1f, worst %.1f -> %.1f' % (
    len(keys), sum(x < -1 for x, _ in d), sum(x > 1 for x, _ in d), statistics.median(a[k] for k in keys), statistics.median(b[k] for k in keys),
    max(a[k] for k in keys), max(b[k] for k in keys)))
for q in d[:8] + [None] + d[-10:]:
    if q is None: print('   ...'); continue
    x, k = q
    print('  %+6.1f: %6.1f -> %6.1f  %s' % (x, a[k], b[k], ' '.join(k)))
