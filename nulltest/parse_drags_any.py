"""Parses MeterCapture drag-script logs generically: for each scripted move, the parameters whose text changed.
    python -I parse_drags_any.py <log>"""
import sys, re


def params(line):
    return dict(re.findall(r'\[([^\]=]+)=([^\]]*)\]', line))


cur, before = None, None
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'== drag (\S+)', line)
    if m: cur = m.group(1); continue
    if cur and line.startswith('before:'): before = params(line)
    elif cur and line.startswith('after:'):
        after = params(line)
        if before is None: before = after
        ch = ['%s %s -> %s' % (k, before[k], after[k]) for k in after if k in before and before[k] != after[k]]
        print('  %-28s %s' % (cur, '; '.join(ch) or '(no change)'))
        before, cur = after, None
