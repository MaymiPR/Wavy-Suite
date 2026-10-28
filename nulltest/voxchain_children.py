"""Prints the reference CLA Vocals chain's child settings by name (research for Phase 4): the default chain
(Presets/1010.xml) and the colours' sub-presets (Presets/1011.xml), each child's values matched to the child's own
ParamXML (in the bundle's PlugIns/<uid>.bundle). Reads the user's install; writes nothing.

    python -I voxchain_children.py [--all]     (--all: every parameter, not only those that differ from the child's init)
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

BUNDLE = r'C:\Program Files (x86)\Waves\Plug-Ins V17\CLA Vocals.bundle\Contents'


def child_params(uid):
    """the child's ParamXML: [(name, unit, init)] in preset-array order"""
    path = os.path.join(BUNDLE, 'PlugIns', '%s.bundle' % uid, 'Contents', 'Resources', 'ParamXML', '1000.xml')
    if not os.path.exists(path): return None
    root = ET.parse(path).getroot()
    out = []
    for p in root.iter():
        if p.tag.lower() not in ('param', 'parameter'): continue
        def t(tag):
            e = p.find(tag)
            return e.text.strip() if e is not None and e.text else ''
        name = p.get('Name', '') or t('ParamName') or t('Name') or t('ID')
        out.append((name or t('ID'), t('Units') or t('Unit') or t('Label'), t('InitialValue') or t('Init') or ''))
    return out


def presets(path):
    """(outer preset name, child tag, child preset name, uid, values) for every child preset in a file"""
    txt = open(path, encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'<PluginSpecific[^>]*TagName="([^"]+)".*?<!\[CDATA\[(.*?)\]\]>', txt, re.S):
        tag, inner = m.group(1), m.group(2)
        name = re.search(r'<Preset Name="([^"]*)"', inner) or re.search(r'Name="([^"]*)"', inner)
        uid = re.search(r'<PluginName>([^<]+)</PluginName>', inner)
        vals = re.search(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', inner)
        yield tag, name.group(1) if name else '?', uid.group(1).strip() if uid else '?', vals.group(1).split() if vals else []


def main():
    show_all = '--all' in sys.argv
    for f in ('1010.xml', '1011.xml'):
        print('=' * 20, f)
        for tag, name, uid, vals in presets(os.path.join(BUNDLE, 'Resources', 'Presets', f)):
            params = child_params(uid) or []
            print('-- %s: "%s" (%s, %d values)' % (tag, name, uid, len(vals)))
            for i, v in enumerate(vals):
                pn, unit, init = params[i] if i < len(params) else ('#%d' % i, '', '')
                if v == '*' or (not show_all and init and _same(v, init)): continue
                print('     %3d %-28s %-14s %s' % (i, pn[:28], v, unit))


def _same(a, b):
    try: return abs(float(a) - float(b)) < 1e-9
    except ValueError: return a == b


if __name__ == '__main__':
    main()
