"""Writes the trained WBass gain network (build/gainnet/gainnet_h<H>.pt, from nulltest/train_gain.py) to
plugins/WBass/WBassGainNet.h as constant arrays. Run in the PyTorch environment.

    python tools/export_gainnet.py <hidden>
"""
import sys, os, math
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
H = int(sys.argv[1]) if len(sys.argv) > 1 else 32
sd = torch.load(os.path.join(HERE, "..", "build", "gainnet", "gainnet_h%d.pt" % H), map_location="cpu")


def arr(name, t):
    v = t.detach().double().flatten().tolist()
    body = ",\n    ".join(", ".join("%.9gf" % x for x in v[i:i + 8]) for i in range(0, len(v), 8))
    return "inline constexpr float %s[%d] = {\n    %s\n};\n" % (name, len(v), body)


out = os.path.join(HERE, "..", "plugins", "WBass", "WBassGainNet.h")
with open(out, "w", newline="\n") as f:
    f.write("#pragma once\n// Learned gain control of the WBass generated path (GRU, %d hidden units), trained on renders of the\n"
            "// reference by nulltest/train_gain.py and written by tools/export_gainnet.py. Do not edit by hand.\n"
            "// Inputs per 48 kHz step: |asinh(v / 1e-3)| / 8 for x, b, l, u, and ln(Freq / 80). Outputs: ln g_pre, ln g_post.\n"
            "// PyTorch GRU layout: rows [reset; update; new].\n\nnamespace wbass::gainnet\n{\n" % H)
    f.write("inline constexpr int hidden = %d, inputs = 5;\n" % H)
    f.write("inline constexpr float C0 = %.9gf, k2 = %.9gf;\n" % (math.exp(sd["logC0"].item()), sd["k2"].item()))
    f.write(arr("Wih", sd["gru.weight_ih_l0"]))
    f.write(arr("Whh", sd["gru.weight_hh_l0"]))
    f.write(arr("bih", sd["gru.bias_ih_l0"]))
    f.write(arr("bhh", sd["gru.bias_hh_l0"]))
    f.write(arr("Wout", sd["out.weight"]))
    f.write(arr("bout", sd["out.bias"]))
    f.write("}\n")
print("wrote", out)
