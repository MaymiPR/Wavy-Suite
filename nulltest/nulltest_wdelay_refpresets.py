"""Null test of WDelay's factory presets against the reference's OWN presets: each preset array of H-Delay's preset file
(on the user's install) written into the reference's state, as the reference loads it (read_ref_presets_hd.py's
method; the state files go to the git-ignored build/hdelay_states/), and that state given to RefRender in the
pre-roll; WDelay's renderer gets the WDelay preset's values at the same time. Unlike nulltest_wdelay_presets.py
(which sets the reference's parameters to WDelay's values), this also catches a preset that WDelay's table carries
over wrongly.

    python -I nulltest_wdelay_refpresets.py [--states] [--only name,...] [--rounded] [--out report.md]
      --states: (re)write the state files first; --rounded: WDelay's renderer rounds the values (as set by a host)
"""
import sys, os, re, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R
import hdelay as H
import nulltest_wdelay as N
import nulltest_wdelay_presets as P

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = r"C:\Program Files (x86)\Waves\Plug-Ins V17\H-Delay.bundle\Contents\Resources\Presets\1000.xml"
STATES = os.path.join(HERE, "..", "build", "hdelay_states")


def write_states():
    import wavesstate as W
    os.makedirs(STATES, exist_ok=True)
    t = open(SRC, encoding="utf-8").read()
    p = R.load("H-Delay Stereo")
    base = p.raw_state
    inner0 = W.get_inner(base).decode("latin-1")
    k = 0
    for m in re.finditer(r'<Preset [^>]*Name="([^"]*)".*?<Parameters Type="(\w+)">([^<]*)</Parameters>', t, re.S):
        name, kind, vals = m.group(1), m.group(2), m.group(3).split()
        for pad in range(16):
            text = ' '.join(vals) + ' ' * (1 + pad)
            inner = re.sub(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', lambda q: '<Parameters Type="%s">%s</Parameters>' % (kind, text), inner0)
            st = W.set_inner(base, inner.encode("latin-1"))
            p.raw_state = st
            got = W.get_values(p.raw_state)
            if got is not None and (got != W.get_values(base) or k == 0): break
        open(os.path.join(STATES, "%02d.bin" % k), "wb").write(bytes(p.raw_state))
        p.raw_state = base
        k += 1
    print("wrote", k, "states")


def state_file(vals, path, kind="RealWorld"):
    """a reference state with this preset array (45 values, '*' keeps the current one) loaded, written to path"""
    import wavesstate as W
    p = R.load("H-Delay Stereo")
    base = p.raw_state
    inner0 = W.get_inner(base).decode("latin-1")
    for pad in range(16):
        text = ' '.join(vals) + ' ' * (1 + pad)
        inner = re.sub(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', lambda q: '<Parameters Type="%s">%s</Parameters>' % (kind, text), inner0)
        p.raw_state = W.set_inner(base, inner.encode("latin-1"))
        if W.get_values(p.raw_state) not in (None, W.get_values(base)): break
    else: raise RuntimeError("state not taken")
    open(path, "wb").write(bytes(p.raw_state))
    p.raw_state = base                                   # (refhost keeps the instance)
    return path


def preset_array(v):
    """a WDelay preset's values as the reference's preset array (positions as tools/make_presets.py reads them)"""
    a = ['*'] * 45
    for i, k in ((1, 'sync'), (2, 'bpm'), (3, 'delay'), (4, 'delayNote'), (6, 'mix'), (7, 'feedback'), (8, 'phaseL'), (9, 'phaseR'),
                 (10, 'pingPong'), (14, 'output'), (15, 'lopass'), (17, 'hipass'), (19, 'depth'), (20, 'rate'), (21, 'lofi'), (23, 'analog')):
        val = 20100.0 if k == 'lopass' and v[k] >= 20100 else v[k]   # the reference's Off is 20100 (20138.77 filters)
        a[i] = repr(float(val)) if k not in ('sync', 'delayNote', 'phaseL', 'phaseR', 'pingPong', 'lofi', 'analog') else str(int(val))
    return a


def render_pair(x, fs, k, v):
    # the preset's Feedback from the input's start (Feedback 0 through the pre-roll, as nulltest_wdelay.py: the
    # reference's first block puts its random Analog noise into the line, which a loop above 100 % would grow)
    r, _ = R.render_ref(x, fs, name=H.NAMES['Stereo'], bpm=120.0, extra=['preroll=3', 'state=' + os.path.abspath(os.path.join(STATES, "%02d.bin" % k)),
                                                                        'Feedback:=0', 'at=0:Feedback:=%.6f' % (v['feedback'] / 200)])
    sync = int(v['sync'])
    kw = dict(mix=v['mix'], feedback=0, changes=[(0.0, 'feedback', v['feedback'])], output=v['output'], hipass=v['hipass'] if v['hipass'] >= 19.5 else 'Off',
              lopass=('Off' if v['lopass'] == 20100 else v['lopass']) if N.EXACT else (v['lopass'] if v['lopass'] < 20100 else 'Off'),
              depth=v['depth'], rate=v['rate'], bpm=v['bpm'],
              lofi=H.CHOICE['lofi'][1][int(v['lofi'])], pingpong=H.CHOICE['pingpong'][1][int(v['pingPong'])],
              phase_l=H.CHOICE['phase_l'][1][int(v['phaseL'])], phase_r=H.CHOICE['phase_r'][1][int(v['phaseR'])],
              analog=H.CHOICE['analog'][1][int(v['analog'])], sync=H.CHOICE['sync'][1][sync], delay=v['delay'],
              note=H.NOTES[int(v['delayNote'])], preroll=3.0)
    return N.ours(x, fs, **kw), r


def main():
    a = sys.argv[1:]
    if '--states' in a or not os.path.exists(os.path.join(STATES, "00.bin")): write_states()
    only = a[a.index('--only') + 1].split(',') if '--only' in a else None
    N.EXACT = '--rounded' not in a                       # a preset's values go in exactly (the reference's preset load)
    out_md = a[a.index('--out') + 1] if '--out' in a else None
    fs = 48000
    S = N.signals(fs)
    lines = ['| group | preset | Analog | ' + ' | '.join(S) + ' |', '|---|---|---|' + '---|' * len(S)]
    for k, (g, name, v) in enumerate(P.presets()):
        if only and name not in only: continue
        row = []
        for sname, x in S.items():
            o, r = render_pair(np.stack([x, 0.8 * x]), fs, k, v)
            row.append('%.1f' % N.residual_db(o, r))
        lines.append('| %s | %s | %s | %s |' % (g, name, ['Off', '1', '2', '3', '4'][int(v['analog'])], ' | '.join(row)))
        print(lines[-1], flush=True)
    if out_md: open(out_md, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
