"""WBassX level control, "det" model: an explicit detector with a learned gain law, trained end-to-end on the
reference's generated part (the alternative to the GRU in train_wbassx.py).

    z = K(F) HP_G(u)                                       the signal the level control listens to
    e1 = attack / release follower of |z|  (a1, r1),  e2 = attack / release follower of e1  (a2, r2)
         time constants tau_k = tau0_k (Response / 20)^beta_k (80 / F)^gamma_k, learned (forward-mode derivatives in C,
         nulltest/mbfeat.c)
    ln g = -softplus( MLP( e1, e2, |z|, ln Ratio, Response, ln(F / 80), High-pass ) )   (per sample; g <= 1)

    python train_wbassx_det.py steps [width] [init.pt]
"""
import sys, os, math, time, json
import numpy as np
import torch
from scipy.signal import sosfilt
import ctypes as ct

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import train_wbassx as T

FS = T.FS; dev = T.dev
_D = ct.CDLL(os.path.join(HERE, "mbfeat.dll"))
_D.mb_follow2.argtypes = [ct.c_void_p, ct.c_int] + [ct.c_double] * 4 + [ct.c_void_p] * 4
TAU0 = (0.001, 0.03, 0.01, 0.06)


class DetFn(torch.autograd.Function):
    @staticmethod
    def forward(ctx, alpha, zs):
        al = alpha.detach().cpu().double().numpy(); B = al.shape[0]; n = len(zs[0])
        E1 = np.empty((B, n)); E2 = np.empty((B, n)); D1 = np.empty((B, n, 2)); D2 = np.empty((B, n, 4))
        for b in range(B):
            e1, e2, d1, d2 = np.empty(n), np.empty(n), np.empty(2 * n), np.empty(4 * n)
            z = np.ascontiguousarray(zs[b], np.float64)
            _D.mb_follow2(z.ctypes.data, n, *al[b], e1.ctypes.data, e2.ctypes.data, d1.ctypes.data, d2.ctypes.data)
            E1[b], E2[b], D1[b], D2[b] = e1, e2, d1.reshape(n, 2), d2.reshape(n, 4)
        ctx.D1, ctx.D2, ctx.dev = D1, D2, alpha.device
        return torch.from_numpy(E1).float().to(alpha.device), torch.from_numpy(E2).float().to(alpha.device)

    @staticmethod
    def backward(ctx, g1, g2):
        g1 = g1.detach().cpu().double().numpy(); g2 = g2.detach().cpu().double().numpy()
        ga = np.einsum("bn,bnk->bk", g2, ctx.D2)
        ga[:, :2] += np.einsum("bn,bnk->bk", g1, ctx.D1)
        return torch.from_numpy(ga).float().to(ctx.dev), None


def feat_t(v): return torch.abs(torch.asinh(v / 1e-3)) / 8.0


class DetNet(torch.nn.Module):
    def __init__(self, width=48):
        super().__init__()
        self.ltau = torch.nn.Parameter(torch.tensor([math.log(t) for t in TAU0]))
        self.beta = torch.nn.Parameter(torch.ones(4))
        self.gamma = torch.nn.Parameter(torch.zeros(4))
        self.rho = torch.nn.Parameter(torch.zeros(4))           # tau also scales with Ratio^rho
        self.mlp = torch.nn.Sequential(torch.nn.Linear(7, width), torch.nn.Tanh(), torch.nn.Linear(width, width), torch.nn.Tanh(),
                                       torch.nn.Linear(width, 1))
        with torch.no_grad(): self.mlp[-1].bias.fill_(1.0)

    def alphas(self, resp, F, ratio):
        tau = (torch.exp(self.ltau)[None, :] * (resp[:, None] / 20.0) ** self.beta[None, :] * (80.0 / F[:, None]) ** self.gamma[None, :]
               * ratio[:, None] ** self.rho[None, :])
        return torch.exp(-1.0 / (tau * FS))

    def forward(self, zs, z, ratio, resp, F, hp):
        e1, e2 = DetFn.apply(self.alphas(resp, F, ratio), zs)
        n = z.shape[1]
        c = lambda v: v[:, None].expand(-1, n)
        x = torch.stack([feat_t(e1), feat_t(e2), feat_t(z), c(torch.log(ratio)), c((resp - 20.0) / 10.0), c(torch.log(F / 80.0)), c(hp - 1.0)], -1)
        return -torch.nn.functional.softplus(self.mlp(x)[..., 0])


def batch(exs, idx):
    us, zs, hks = [], [], []
    for i in idx:
        e = exs[i]
        key = (int(e["F"]), e["hp"])
        if key not in T._hk: T._hk[key] = T.hpg_ir(e)
        u = sosfilt(T.lp3(e["F"]), e["x"].astype(np.float64))
        zs.append(T.KT[int(e["F"])] * sosfilt(T.g_highpass(e["F"], e["hp"]), u)); us.append(u.astype(np.float32)); hks.append(T._hk[key])
    tt = lambda v: torch.tensor(v, dtype=torch.float32, device=dev)
    return (zs, torch.from_numpy(np.stack(zs).astype(np.float32)).to(dev), torch.from_numpy(np.stack(us)).to(dev),
            torch.from_numpy(np.stack(hks)).to(dev), torch.from_numpy(np.stack([exs[i]["G"] for i in idx]).astype(np.float32)).to(dev),
            [exs[i]["coefs"] for i in idx], tt([exs[i]["ratio"] for i in idx]), tt([exs[i]["response"] for i in idx]),
            tt([exs[i]["F"] for i in idx]), tt([float(exs[i]["hp"]) for i in idx]))


def out(net, b):
    zs, z, u, hk, G, co, ratio, resp, F, hp = b
    w = torch.exp(net(zs, z, ratio, resp, F, hp)) * u
    return T.fftconv(T.GenFn.apply(w, co), hk), G


def evaluate(net, exs, bs=16):
    net.eval(); res = []
    with torch.no_grad():
        for s in range(0, len(exs), bs):
            y, G = out(net, batch(exs, list(range(s, min(len(exs), s + bs)))))
            res += T.e2e_loss(y, G).tolist()
    net.train()
    return np.array(res)


def main():
    steps = int(sys.argv[1]); width = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    init = sys.argv[3] if len(sys.argv) > 3 else None
    tag = os.environ.get("MB_TAG", "")
    tr, va = T.load_light("train"), T.load_light("val")
    for e in tr + va: e["coefs"] = T.gen_coefs(e)
    print(len(tr), "train", len(va), "val (det model)", flush=True)
    net = DetNet(width).to(dev)
    if init: net.load_state_dict(torch.load(init), strict=False)
    lr = float(os.environ.get("MB_LR", "2e-3"))
    opt = torch.optim.Adam(net.parameters(), lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, steps)
    rng = np.random.default_rng(int(os.environ.get("MB_SEED", "1"))); best = 1e9; t0 = time.time()
    path = os.path.join(T.OUTD, f"det_w{width}{tag}.pt")
    for it in range(1, steps + 1):
        y, G = out(net, batch(tr, rng.choice(len(tr), 16, replace=False)))
        loss = T.e2e_loss(y, G).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0); opt.step(); sched.step()
        if it % 200 == 0 or it == steps:
            v = evaluate(net, va)
            tau = (torch.exp(net.ltau) * 1000).tolist()
            print(f"{it:6d} train {loss.item():6.2f}  val median {np.median(v):6.2f} mean {v.mean():6.2f}  tau ms {[round(t, 2) for t in tau]} "
                  f"beta {[round(b, 2) for b in net.beta.tolist()]} gamma {[round(g, 2) for g in net.gamma.tolist()]} rho {[round(r, 2) for r in net.rho.tolist()]} ({time.time() - t0:.0f} s)", flush=True)
            if np.median(v) < best: best = np.median(v); torch.save(net.state_dict(), path)
    net.load_state_dict(torch.load(path)); v = evaluate(net, va); by = {}
    for e, d in zip(va, v): by.setdefault(e["kind"], []).append(d)
    print("best val median %.2f; by kind:" % np.median(v), {k: round(float(np.median(d)), 1) for k, d in by.items()}, flush=True)


if __name__ == "__main__":
    main()
