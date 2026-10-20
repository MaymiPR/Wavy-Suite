"""Learned level control for WBassX (gray-box): the identified structure of the generated part stays fixed and exact
(MEASUREMENTS.md "WBassX model"), only the gain g(t) of the band signal u = LP3(F) x is learned:

    ln g = -softplus(Linear(GRU(features)))       (g <= 1: the measured gain cap)
    features: compressed |x|, |u|, ln Ratio, (Response - 20) / 10, ln(F / 80)    (magnitudes only: the gain is symmetric)

Targets: the gain extracted from reference renders (render_trainset_mb.py, extract_g2.py). Loss: the gain error weighted
by u^2, i.e. by how much it changes the output (where u is near zero the extracted gain is not determined).
Runs in the PyTorch environment (C:\\Users\\wint1\\.venvs\\wavy-torch).

    python train_wbassx.py [steps] [hidden]                      (supervised on the extracted gains)
    python train_wbassx.py e2e steps hidden [init.pt]           (end-to-end on the generated part itself)
"""
import sys, os, glob, json, math, time
import numpy as np
from scipy.signal import sosfilt
import torch

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "measurements", "maxxbass", "train")
OUTD = os.path.join(HERE, "..", "build", "wbassxnet")
os.makedirs(OUTD, exist_ok=True)
dev = torch.device("cuda")
SKIP = FS // 20


def lp3(F):
    k = math.tan(math.pi * F / FS); n1 = 1 / (1 + k); k2 = k * k; n2 = 1 / (1 + k + k2)
    return np.array([[k * n1, k * n1, 0, 1, (k - 1) * n1, 0], [k2 * n2, 2 * k2 * n2, k2 * n2, 1, 2 * (k2 - 1) * n2, (1 - k + k2) * n2]])


def feat(v): return np.abs(np.arcsinh(v / 1e-3)) / 8.0


FEAT = os.environ.get("MB_FEAT", "base")
ZENV_PEAK, ZENV_POW = (0.003, 0.01, 0.03, 0.1), (0.01, 0.05)
import ctypes as _ctf
_F = _ctf.CDLL(os.path.join(HERE, "mbfeat.dll")) if os.path.exists(os.path.join(HERE, "mbfeat.dll")) else None
if _F is not None:
    _F.mb_peak.argtypes = [_ctf.c_void_p, _ctf.c_void_p, _ctf.c_int, _ctf.c_double]
    _F.mb_power.argtypes = [_ctf.c_void_p, _ctf.c_void_p, _ctf.c_int, _ctf.c_double]       # "env": three peak followers of |u| as extra inputs
ENV_REL = (0.005, 0.03, 0.2)                     # release times (s); instant attack


def peak_follow(v, rel):
    from scipy.signal import lfilter
    a = math.exp(-1.0 / (rel * FS)); out = np.empty_like(v); e = 0.0
    for i, x in enumerate(v):                    # (python loop: precomputed once per example and cached)
        e = x if x > e else a * e + (1 - a) * x
        out[i] = e
    return out


def features(x, u, F, ratio, response, hp=1):
    n = len(x)
    cols = [feat(x), feat(u), np.full(n, math.log(ratio)), np.full(n, (response - 20.0) / 10.0), np.full(n, math.log(F / 80.0))]
    if FEAT in ("zlin", "zenv"):
        # the level control listens after the generated part's high-pass: give it u through K(F) HP_G (and the setting)
        z = KT[int(F)] * sosfilt(g_highpass(F, hp), u)
        cols += [feat(z), np.full(n, float(hp - 1))]
    if FEAT == "zenv":
        # envelope followers of |z| (instant attack, release 3 / 10 / 30 / 100 ms) and power smoothers (10 / 50 ms): the
        # slow time constants a GRU stepping at 48 kHz finds hard to hold
        z = np.ascontiguousarray(z, np.float64)
        for rel in ZENV_PEAK:
            y = np.empty(n); _F.mb_peak(z.ctypes.data, y.ctypes.data, n, math.exp(-1.0 / (rel * FS))); cols.append(feat(y))
        for tau in ZENV_POW:
            y = np.empty(n); _F.mb_power(z.ctypes.data, y.ctypes.data, n, math.exp(-1.0 / (tau * FS))); cols.append(feat(np.sqrt(y)))
    if FEAT == "env":
        au = np.abs(u)
        cols += [feat(peak_follow(au, r)) for r in ENV_REL]
    return np.stack(cols, 1).astype(np.float32)


def load(split, max_err=-20.0, need_g=True):
    ex = []
    for f in sorted(glob.glob(os.path.join(DATA, split + "_*.npz"))):
        z = np.load(f)
        if need_g and (z["g"].shape[0] < 2 or float(z["err"]) > max_err): continue
        x = z["x"].astype(np.float64); F = float(z["F"]); u = sosfilt(lp3(F), x)
        ex.append(dict(name=os.path.basename(f), kind=str(z["kind"]), F=F, fe=features(x, u, F, float(z["ratio"]), float(z["response"])),
                       u=u.astype(np.float32), g=(z["g"] if z["g"].shape[0] == len(x) else np.zeros(len(x))).astype(np.float32), err=float(z["err"]), G=z["G"].astype(np.float32),
                       decay=float(z["decay"]), hp=int(z["hp"])))
    return ex


class Net(torch.nn.Module):
    def __init__(self, hidden=32, inputs=5):
        super().__init__()
        self.gru = torch.nn.GRU(inputs, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, 1)
        with torch.no_grad(): self.out.bias.fill_(1.0)

    def forward(self, fe):
        hs, h = [], None
        for c in range(0, fe.shape[1], 8192):          # cuDNN limits the sequence length per call; carry the state
            y, h = self.gru(fe[:, c:c + 8192].contiguous(), h)
            hs.append(y)
        return -torch.nn.functional.softplus(self.out(torch.cat(hs, 1))[..., 0])      # ln g


def batch(exs, idx):
    fe = torch.from_numpy(np.stack([exs[i]["fe"] for i in idx])).to(dev)
    u = torch.from_numpy(np.stack([exs[i]["u"] for i in idx])).to(dev)
    g = torch.from_numpy(np.stack([exs[i]["g"] for i in idx])).to(dev)
    return fe, u, g


def loss_db(lng, u, g):
    """per example: 10 log10( sum u^2 (g_pred - g)^2 / sum u^2 g^2 ), the relative error of the gained band signal"""
    e = ((torch.exp(lng) - g) * u)[:, SKIP:]; r = (g * u)[:, SKIP:]
    return 10 * torch.log10((e * e).sum(1) / (r * r).sum(1) + 1e-12)


def evaluate(net, exs, bs=16):
    net.eval(); out = []
    with torch.no_grad():
        for s in range(0, len(exs), bs):
            idx = list(range(s, min(len(exs), s + bs)))
            fe, u, g = batch(exs, idx)
            out += loss_db(net(fe), u, g).tolist()
    net.train()
    return np.array(out)


# ---- end-to-end: the loss on the generated part itself, through the identified structure ---------------------------
import ctypes as ct
_J = ct.CDLL(os.path.join(HERE, "mbfast.dll"))      # build_mbfast.bat
_P = ct.c_void_p
_J.mbp_traj.argtypes = [_P, _P, _P, _P, ct.c_int] + [ct.c_double] * 7
_J.mbp_vjp.argtypes = [_P, _P, _P, _P, ct.c_int] + [ct.c_double] * 5


def sos2(fc, q, high):
    k = math.tan(math.pi * fc / FS); k2 = k * k; n = 1 / (1 + k / q + k2)
    b = [n, -2 * n, n] if high else [k2 * n, 2 * k2 * n, k2 * n]
    return np.array(b + [1, 2 * (k2 - 1) * n, (1 - k / q + k2) * n])


def g_highpass(F, hp):
    q = 2 ** -0.5
    if hp == 0: return np.stack([sos2(16, q, True)] * 2)
    if hp == 1: return np.stack([sos2(F, q, True), sos2(16, q, True)])
    return np.stack([sos2(0.8 * F, q, True)] * 2)


TAG = os.environ.get("MB_TAG", "")
KT = {int(k): v[0] for k, v in json.load(open(os.path.join(HERE, "..", "measurements", "maxxbass", "k_out_table.json"))).items()}
IRL = 1 << 15


class GenFn(torch.autograd.Function):
    """y = w (1 + a v[n-1]), v = HP_B(y), |y| <= 1: forward and exact adjoint in C (mbfast.c), per example"""
    @staticmethod
    def forward(ctx, w, coefs):
        wn = w.detach().cpu().double().numpy(); B, n = wn.shape
        y = np.empty_like(wn); dw = np.empty_like(wn); dz = np.empty_like(wn)
        for i in range(B):
            a, c = coefs[i]
            wi = np.ascontiguousarray(wn[i]); yi = np.empty(n); dwi = np.empty(n); dzi = np.empty(n)
            _J.mbp_traj(wi.ctypes.data, yi.ctypes.data, dwi.ctypes.data, dzi.ctypes.data, n, a, *c, 1.0)
            y[i], dw[i], dz[i] = yi, dwi, dzi
        ctx.save_for_backward(torch.from_numpy(dw), torch.from_numpy(dz)); ctx.coefs = coefs; ctx.dev = w.device
        return torch.from_numpy(y).float().to(w.device)

    @staticmethod
    def backward(ctx, gy):
        dw, dz = ctx.saved_tensors; gn = gy.detach().cpu().double().numpy(); out = np.empty_like(gn)
        for i in range(gn.shape[0]):
            a, c = ctx.coefs[i]
            gi = np.ascontiguousarray(gn[i]); oi = np.empty(gi.shape[0])
            dwi = np.ascontiguousarray(dw[i].numpy()); dzi = np.ascontiguousarray(dz[i].numpy())
            _J.mbp_vjp(dwi.ctypes.data, dzi.ctypes.data, gi.ctypes.data, oi.ctypes.data, gi.shape[0], *c)
            out[i] = oi
        return torch.from_numpy(out).float().to(ctx.dev), None


def gen_coefs(e):
    s = sos2(min(max(e["F"] / 3.0, 20.0), 40.0), 2 ** -0.5, True)
    return (8.0 * 10 ** (e["decay"] / 20), (s[0], s[1], s[2], s[4], s[5]))


def hpg_ir(e):
    d = np.zeros(IRL); d[0] = 1.0
    return (-KT[int(e["F"])] * sosfilt(g_highpass(e["F"], e["hp"]), d)).astype(np.float32)


def fftconv(sig, kern):
    n = sig.shape[-1] + kern.shape[-1]; L = 1 << (n - 1).bit_length()
    return torch.fft.irfft(torch.fft.rfft(sig, L) * torch.fft.rfft(kern, L), L)[..., :sig.shape[-1]]


_hk, _lp = {}, {}
def load_light(split):
    """x and G only (features and filters are made per batch)"""
    ex = []
    for f in sorted(glob.glob(os.path.join(DATA, split + "_*.npz"))):
        z = np.load(f)
        ex.append(dict(name=os.path.basename(f), kind=str(z["kind"]), F=float(z["F"]), ratio=float(z["ratio"]), response=float(z["response"]),
                       decay=float(z["decay"]), hp=int(z["hp"]), x=z["x"].astype(np.float32), G=z["G"].astype(np.float16)))
    return ex


def prep(e):
    key = (int(e["F"]), e["hp"])
    if key not in _hk: _hk[key] = hpg_ir(e)
    if "fe" in e: return e["fe"], e["u"], _hk[key]
    x = e["x"].astype(np.float64); u = sosfilt(lp3(e["F"]), x)
    fe = features(x, u, e["F"], e["ratio"], e["response"], e["hp"])
    if FEAT == "env": e["fe"], e["u"] = fe.astype(np.float16), u.astype(np.float32)     # cache (half precision features)
    return fe, u.astype(np.float32), _hk[key]


def e2e_batch(exs, idx):
    pp = [prep(exs[i]) for i in idx]
    fe = torch.from_numpy(np.stack([p[0] for p in pp]).astype(np.float32)).to(dev)
    u = torch.from_numpy(np.stack([p[1] for p in pp])).to(dev)
    hk = torch.from_numpy(np.stack([p[2] for p in pp])).to(dev)
    G = torch.from_numpy(np.stack([exs[i]["G"] for i in idx]).astype(np.float32)).to(dev)
    return fe, u, hk, G, [exs[i]["coefs"] for i in idx]


def e2e_out(net, fe, u, hk, coefs):
    w = torch.exp(net(fe)) * u
    return fftconv(GenFn.apply(w, coefs), hk)


def e2e_loss(y, G):
    e = (y - G)[:, SKIP:]; r = G[:, SKIP:]
    return 10 * torch.log10((e * e).sum(1) / (r * r).sum(1) + 1e-12)


def e2e_eval(net, exs, bs=16):
    net.eval(); out = []
    with torch.no_grad():
        for s in range(0, len(exs), bs):
            idx = list(range(s, min(len(exs), s + bs)))
            fe, u, hk, G, co = e2e_batch(exs, idx)
            out += e2e_loss(e2e_out(net, fe, u, hk, co), G).tolist()
    net.train()
    return np.array(out)


def main_e2e(steps, hidden, init):
    tr, va = load_light("train"), load_light("val")
    for e in tr + va:
        e["coefs"] = gen_coefs(e)
    print(len(tr), "train", len(va), "val (end-to-end)", flush=True)
    nin = {"env": 8, "zlin": 7, "zenv": 13}.get(FEAT, 5)
    net = Net(hidden, nin).to(dev)
    if init:
        sd = torch.load(init)
        w = sd["gru.weight_ih_l0"]
        if w.shape[1] < nin:
            sd["gru.weight_ih_l0"] = torch.cat([w, torch.zeros(w.shape[0], nin - w.shape[1], device=w.device)], 1)
        h0 = sd["gru.weight_hh_l0"].shape[1]
        if h0 < hidden:
            # grow the hidden layer without changing the function: new units take small random input weights, but no old
            # unit reads them and the output ignores them at first
            cur = net.state_dict()
            for k in ("gru.weight_ih_l0", "gru.weight_hh_l0", "gru.bias_ih_l0", "gru.bias_hh_l0"):
                old, new = sd[k].to(dev), cur[k].clone() * 0.3
                for gte in range(3):
                    rows_o, rows_n = slice(gte * h0, (gte + 1) * h0), slice(gte * hidden, gte * hidden + h0)
                    if old.dim() == 1: new[rows_n] = old[rows_o]
                    elif k.endswith("hh_l0"): new[rows_n, :h0] = old[rows_o]; new[rows_n, h0:] = 0.0
                    else: new[rows_n] = old[rows_o]
                sd[k] = new
            w = torch.zeros(1, hidden, device=dev); w[:, :h0] = sd["out.weight"].to(dev); sd["out.weight"] = w
        net.load_state_dict(sd)
    v = e2e_eval(net, va); print(f"start: val median {np.median(v):.2f} mean {v.mean():.2f}", flush=True)
    opt = torch.optim.Adam(net.parameters(), float(os.environ.get("MB_LR", "1e-3")))
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, steps)
    rng = np.random.default_rng(int(os.environ.get("MB_SEED", "1"))); best = np.median(v); t0 = time.time()
    torch.save(net.state_dict(), os.path.join(OUTD, f"net_h{hidden}_e2e{TAG}.pt"))
    for it in range(1, steps + 1):
        idx = rng.choice(len(tr), 16, replace=False)
        fe, u, hk, G, co = e2e_batch(tr, idx)
        loss = e2e_loss(e2e_out(net, fe, u, hk, co), G).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0); opt.step(); sched.step()
        if it % 100 == 0 or it == steps:
            v = e2e_eval(net, va)
            print(f"{it:5d} train {loss.item():6.2f} dB  val median {np.median(v):6.2f} mean {v.mean():6.2f} worst {v.max():6.2f}  ({time.time() - t0:.0f} s)", flush=True)
            if np.median(v) < best:
                best = np.median(v); torch.save(net.state_dict(), os.path.join(OUTD, f"net_h{hidden}_e2e{TAG}.pt"))
    net.load_state_dict(torch.load(os.path.join(OUTD, f"net_h{hidden}_e2e{TAG}.pt")))
    v = e2e_eval(net, va); by = {}
    for e, d in zip(va, v): by.setdefault(e["kind"], []).append(d)
    print("best val median %.2f; by kind:" % np.median(v), {k: round(float(np.median(d)), 1) for k, d in by.items()}, flush=True)


def main():
    steps = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    hidden = int(sys.argv[2]) if len(sys.argv) > 2 else 32
    tr, va = load("train"), load("val")
    print(len(tr), "train", len(va), "val", flush=True)
    torch.manual_seed(0)
    net = Net(hidden).to(dev)
    opt = torch.optim.Adam(net.parameters(), 3e-3)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, 3e-3, total_steps=steps, pct_start=0.1)
    rng = np.random.default_rng(0); best = 1e9; t0 = time.time()
    for it in range(steps):
        idx = rng.choice(len(tr), 24, replace=False)
        fe, u, g = batch(tr, idx)
        loss = loss_db(net(fe), u, g).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0); opt.step(); sched.step()
        if it % 200 == 0 or it == steps - 1:
            v = evaluate(net, va)
            print(f"{it:5d} train {loss.item():6.2f} dB  val median {np.median(v):6.2f} mean {v.mean():6.2f} worst {v.max():6.2f}  ({time.time() - t0:.0f} s)", flush=True)
            if np.median(v) < best:
                best = np.median(v); torch.save(net.state_dict(), os.path.join(OUTD, f"net_h{hidden}.pt"))
    net.load_state_dict(torch.load(os.path.join(OUTD, f"net_h{hidden}.pt")))
    v = evaluate(net, va)
    by = {}
    for e, d in zip(va, v): by.setdefault(e["kind"], []).append(d)
    print("val by kind:", {k: round(float(np.median(d)), 1) for k, d in by.items()})
    json.dump(dict(hidden=hidden, val_median=float(np.median(v)), val=list(map(float, v))), open(os.path.join(OUTD, f"result_h{hidden}.json"), "w"))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "e2e":
        main_e2e(int(sys.argv[2]), int(sys.argv[3]), sys.argv[4] if len(sys.argv) > 4 else None)
    else:
        main()
