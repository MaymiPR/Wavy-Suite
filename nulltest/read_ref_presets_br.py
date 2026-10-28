"""Reads the reference Bass Rider presets (Bass Rider.bundle/Contents/Resources/Presets/1000.xml on the user's install)
and writes their stored values to build/ref_presets_BassRider.json. The arrays are RealWorld values in the preset-array
order of RESEARCH.md section 6 (4 Output, 8 / 9 Range Min / Max, 12 Target or "*" (kept), 31 Sensitivity, 33 Response,
34 the Idle arrow's place in the range, 44 Artifacts as its -20 .. -3 dB gate, 45 Spill). Nothing from the file is
published; WBass Rider presets are made from the values with new names (tools/make_presets.py)."""
import os, re, json

SRC = r"C:\Program Files (x86)\Waves\Plug-Ins V17\Bass Rider.bundle\Contents\Resources\Presets\1000.xml"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ref_presets_BassRider.json")
t = open(SRC, encoding="utf-8").read()
out = []
for m in re.finditer(r'<Preset [^>]*Name="([^"]*)".*?<Parameters Type="(\w+)">([^<]*)</Parameters>', t, re.S):
    assert m.group(2) == "RealWorld", m.group(1)
    out.append(dict(name=m.group(1), raw=m.group(3).split()))
json.dump(out, open(OUT, "w"), indent=1)
print("wrote", OUT, len(out))
