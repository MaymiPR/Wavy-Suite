"""Scores a trained WVox slow-path network (nulltest/train_wvox.py) on the standard null-test signals
(nulltest_wvox.signals) against the reference, before it is ported to C++. Gate off, Gain 0.

    python -I eval_wvoxnet.py render                                  (reference renders, needs pedalboard)
    python eval_wvoxnet.py build/wvoxnet/wvoxnet_h48r3.pt 48          (scores, PyTorch environment)
"""
import sys, os, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
try:
    import pedalboard  # noqa: F401
except ImportError:          # the PyTorch environment: the null-test helpers only need numpy here
    import types; sys.modules["refhost"] = types.ModuleType("refhost")
import nulltest_wvox as N

CASES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "wvoxnet", "evalset.npz")

FS = 48000


def internals(x, comp):
    fi, fo = os.path.join(N.TMP, "ein.f32"), os.path.join(N.TMP, "eout.f32")
    x.astype(np.float32).tofile(fi)
    subprocess.run([N.EXE, fi, fo, "1", str(FS), str(comp), "-80", "0", "dump=1"], check=True, capture_output=True)
    return np.fromfile(fo, np.float32), np.fromfile(fo + ".trace", np.float32).reshape(-1, 4)


if __name__ == "__main__" and sys.argv[1] == "render":
    import refhost as R
    out = {}
    for k, (name, x) in enumerate(N.signals().items()):
        for comp in (-12, -24, -36):
            ref = R.render(x, FS, comp=comp)[0].astype(np.float32)
            ours, tr = internals(x, comp)
            out["c%d_%d" % (k, -comp)] = np.array([name, comp], dtype=object)
            out["x%d_%d" % (k, -comp)] = x; out["r%d_%d" % (k, -comp)] = ref; out["o%d_%d" % (k, -comp)] = ours; out["t%d_%d" % (k, -comp)] = tr
    np.savez(CASES, **out)
    print("rendered", len(out) // 5, "cases")
elif __name__ == "__main__":
    import torch
    import train_wvox as T
    net = T.Net(int(sys.argv[2])).to(T.dev); net.load_state_dict(torch.load(sys.argv[1])); net.eval()
    z = np.load(CASES, allow_pickle=True)
    rows = []
    for key in sorted(k[1:] for k in z.files if k[0] == "c"):
        name, comp = z["c" + key]; comp = float(comp)
        x, ref, ours, tr = z["x" + key], z["r" + key], z["o" + key], z["t" + key]
        Tn = (len(tr) // T.D) * T.D; idle = 0.99 * 10 ** (-comp / 20)
        xd = np.zeros(Tn, np.float32); xd[T.LAT:] = x[:Tn - T.LAT] * idle
        fe, lnF, lnS = T.prepare(tr[:, 0], tr[:, 1], tr[:, 2], comp)
        g = lambda a: torch.tensor(np.ascontiguousarray(a[None], dtype=np.float32)).to(T.dev)
        with torch.no_grad():
            y, _ = net(g(fe), g(lnF), g(lnS), g(xd), 0.0)
        y = y[0].cpu().numpy()[T.LAT:T.LAT + len(x)]
        a, b = N.residual_db(ours, ref, int(0.2 * FS)), N.residual_db(y, ref[:len(y)], int(0.2 * FS))
        rows.append((name, comp, a, b)); print("%-26s comp %4d  analytical %7.1f  learned %7.1f dB" % rows[-1], flush=True)
    a = np.array([r[2] for r in rows]); b = np.array([r[3] for r in rows])
    print("median analytical %.1f, learned %.1f; worst %.1f -> %.1f; learned better in %d of %d" % (np.median(a), np.median(b), a.max(), b.max(), (b < a).sum(), len(a)))
