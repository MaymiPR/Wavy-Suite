"""Reads the reference Vocal Rider presets (Vocal Rider.bundle/Contents/Resources/Presets/1000.xml on the user's
install) and writes their stored arrays to build/ref_presets_VocalRider.json. The arrays are RealWorld values in the
ParamXML order of RESEARCH.md section 7 (0 Target, 1 Music Sensitivity, 2 / 3 Range Min / Max, 6 Vocal Sensitivity,
20 Output, 30 Automation or "*" (kept), 32 the Idle arrow's place in the range, 42 Attack; the Attack-tied hidden
values follow Attack). Nothing from the file is published; WVox Rider presets are made from the values with new names
(tools/make_presets.py)."""
import os, re, json

SRC = r"C:\Program Files (x86)\Waves\Plug-Ins V17\Vocal Rider.bundle\Contents\Resources\Presets\1000.xml"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ref_presets_VocalRider.json")
t = open(SRC, encoding="utf-8").read()
out = []
for m in re.finditer(r'<Preset [^>]*Name="([^"]*)".*?<Parameters Type="(\w+)">([^<]*)</Parameters>', t, re.S):
    assert m.group(2) == "RealWorld", m.group(1)
    out.append(dict(name=m.group(1), raw=m.group(3).split()))
json.dump(out, open(OUT, "w"), indent=1)
print("wrote", OUT, len(out))
