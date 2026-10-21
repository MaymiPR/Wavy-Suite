"""Reads the reference MaxxBass presets (MaxxBass.bundle/Contents/Resources/Presets/1000.xml on the user's install)
and writes their settings, converted by the reference itself, to build/ref_presets_MaxxBass.json: each preset array is
written into the loaded plug-in's state (as stored, States or RealWorld) and the plug-in's own parameter values are
read back. Nothing from the file is published; WBassX presets are made from the values with new names
(tools/make_presets.py)."""
import sys, os, re, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refhost as R
import wavesstate as W

SRC = r"C:\Program Files (x86)\Waves\Plug-Ins V17\MaxxBass.bundle\Contents\Resources\Presets\1000.xml"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ref_presets_MaxxBass.json")
import math


def gain_db(v):
    """the reference's gain fader taper (WBassX Phase 2): 40 log10(1.5 v) up to 0 dB at 2/3, then 8u + 4u^2, u = 3v - 2"""
    if v <= 0: return float("-inf")
    return 40 * math.log10(1.5 * v) if v <= 2 / 3 else 8 * (3 * v - 2) + 4 * (3 * v - 2) ** 2


def by_formula(kind, vals):
    """the same conversion from the identified scales (States 0..1000, '*' = not stored)"""
    if kind == "RealWorld":
        f = lambda i: float(vals[i])
        return dict(frequency=f(1), harmonics_decay=f(2), response=f(3), ratio=f(4), highpass=int(f(10)) % 3,
                    orig_bass_gain=f(12), harmonics_gain=f(13))
    s = lambda i: float(vals[i]) / 1000.0
    return dict(frequency=round(32 * 8 ** s(1)), harmonics_decay=round(-24 + 15 * s(2), 1), response=round(10 + 20 * s(3), 1),
                ratio=round(4 ** s(4), 2), highpass=int(vals[10]) % 3, orig_bass_gain=round(gain_db(s(12)), 1),
                harmonics_gain=round(gain_db(s(13)), 1))


def from_strings(rw):
    num = lambda t: float("-inf") if "inf" in t else float(t.split()[0].replace(":1", ""))
    return dict(frequency=num(rw["frequency"]), harmonics_decay=num(rw["harmonics_decay"]), response=num(rw["response"]),
                ratio=num(rw["ratio"]), highpass=["6dB/Octave", "12dB/Octave", "24dB/Octave"].index(rw["highpass"]),
                orig_bass_gain=num(rw["orig_bass_gain"]), harmonics_gain=num(rw["harmonics_gain"]))


PARAMS = ("frequency", "harmonics_decay", "response", "ratio", "highpass", "orig_bass_gain", "harmonics_gain", "monitor")

t = open(SRC, encoding="utf-8").read()
p = R.load("MaxxBass Stereo")
base = p.raw_state
inner0 = W.get_inner(base).decode("latin-1")
assert re.search(r'<Parameters Type="RealWorld">', inner0), inner0[:400]
out = []
for m in re.finditer(r'<Preset [^>]*Name="([^"]*)".*?<Parameters Type="(\w+)">([^<]*)</Parameters>', t, re.S):
    name, kind, vals = m.group(1), m.group(2), m.group(3).split()
    vals = ['*' if v == '-1' and kind == "States" else v for v in vals]
    done = None
    for pad in range(16):
        text = ' '.join(vals) + ' ' * (1 + pad)
        inner = re.sub(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', lambda q: '<Parameters Type="%s">%s</Parameters>' % (kind, text), inner0)
        p.raw_state = W.set_inner(base, inner.encode("latin-1"))
        got = W.get_values(p.raw_state)
        if got is not None and (got != W.get_values(base) or name.endswith("Full Reset")):
            done = got; break
    form = by_formula(kind, vals)
    if done is not None:
        ref = from_strings({k: p.parameters[k].string_value.strip() for k in PARAMS})
        bad = {k: (ref[k], form[k]) for k in ref if not (ref[k] == form[k] or abs(ref[k] - form[k]) < 0.051)}
        assert not bad, (name, bad)                     # the formulas reproduce the reference's own conversion
        settings, how = ref, "reference"
    else:
        settings, how = form, "formula (the reference did not take this state)"
    out.append(dict(name=name, kind=kind, raw=vals, settings=settings, converted_by=how))
    print("%-24s %-9s %s %s" % (name, how[:9], settings, ""), flush=True)
    p.raw_state = base
json.dump(out, open(OUT, "w"), indent=1)
