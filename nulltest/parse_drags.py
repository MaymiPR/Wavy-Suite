"""Parses MeterCapture drag-script logs: for each drag, the knob's parameter before and after, as text values.
    python -I parse_drags.py <log> [<log> ...]"""
import sys, re
KN = {(88, 80): 'Analog', (554, 80): 'Output', (86, 218): 'Ratio', (553, 218): 'Mix', (321, 333): 'Threshold',
      (58, 344): 'Punch', (163, 345): 'Attack', (480, 344): 'Release'}
for fn in sys.argv[1:]:
    print('==', fn)
    txt = open(fn, encoding='utf-8', errors='replace').read().split('\n')
    cur = None
    for line in txt:
        m = re.match(r'== drag (\d+),(\d+),(-?\d+),(-?\d+),(\w*),(\d+)', line)
        if m:
            x, y, dx, dy, mod, steps = m.groups(); name = KN.get((int(x), int(y)))
            if name is None: name = min(KN.items(), key=lambda kv: abs(kv[0][0] - int(x)) + abs(kv[0][1] - int(y)))[1]
            cur = [name, int(dy), mod or 'plain', int(steps)]
            continue
        for tag in ('before:', 'after:'):
            if cur is not None and line.startswith(tag):
                v = re.search(r'\[%s=([^\]]*)\]' % cur[0], line)
                cur.append(v.group(1) if v else '?')
                if tag == 'after:' and len(cur) >= 6:
                    print('  %-9s dy %+4d %-5s steps %3d (%3d px per step): %s -> %s' % (cur[0], cur[1], cur[2], cur[3], abs(cur[1]) // cur[3], cur[4], cur[5]))
                    cur = None
