"""Reads the reference H-Delay presets (H-Delay.bundle/Contents/Resources/Presets/1000.xml on the user's install) and
writes their settings, converted by the reference itself, to build/ref_presets_HDelay.json: each preset array is
written into the loaded plug-in's state and the plug-in's own parameter values are read back. Nothing from the file is
published; WDelay presets are made from the values with new names (tools/make_presets.py)."""
import sys, os, re, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refhost as R
import wavesstate as W

SRC = r"C:\Program Files (x86)\Waves\Plug-Ins V17\H-Delay.bundle\Contents\Resources\Presets\1000.xml"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ref_presets_HDelay.json")
PARAMS = ("delay_sec", "delay_bpm", "feedback", "pingpong", "sync", "mix", "output", "analog", "lofi", "rate", "depth",
          "hipass", "lopass", "phase_l", "phase_r", "bpm", "ping_pong_start_direction", "delay_sec_r", "delay_bpm_r")


def num(t):
    t = t.strip().split()[0] if t.strip() else t
    try: return float(t)
    except ValueError: return t.strip()


t = open(SRC, encoding="utf-8").read()
p = R.load("H-Delay Stereo")
base = p.raw_state
inner0 = W.get_inner(base).decode("latin-1")
assert re.search(r'<Parameters Type="RealWorld">', inner0), inner0[:400]
out = []
for m in re.finditer(r'<Preset [^>]*Name="([^"]*)".*?<Parameters Type="(\w+)">([^<]*)</Parameters>', t, re.S):
    name, kind, vals = m.group(1), m.group(2), m.group(3).split()
    done = None
    for pad in range(16):
        text = ' '.join(vals) + ' ' * (1 + pad)
        inner = re.sub(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', lambda q: '<Parameters Type="%s">%s</Parameters>' % (kind, text), inner0)
        p.raw_state = W.set_inner(base, inner.encode("latin-1"))
        got = W.get_values(p.raw_state)
        if got is not None and (got != W.get_values(base) or name == "Full Reset"):
            done = got; break
    if done is None:
        print("%-32s NOT taken by the reference" % name, flush=True)
        out.append(dict(name=name, raw=vals, settings=None)); continue
    st = {k: p.parameters[k].string_value.strip() for k in PARAMS}
    out.append(dict(name=name, raw=vals, strings=st, settings={k: num(v) for k, v in st.items()}))
    print("%-32s %s" % (name, st), flush=True)
    p.raw_state = base
json.dump(out, open(OUT, "w"), indent=1)
print("wrote", OUT, len(out))
