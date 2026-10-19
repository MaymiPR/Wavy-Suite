"""Learned slow path for WVox (gray-box): the identified detector, fast path (look-ahead) and static law stay exact;
a small GRU corrects the analytical slow ratio.

    S = S_analytical * exp(c)           c: GRU output (one step per 4 samples at 48 kHz, 12 kHz clock), interpolated
                                           between the last two finished steps (causal, as in the plug-in)
    g = min(F, S)                       (the soft min's gradient while training, the exact min forward)
    y = idle * x_delayed * g            idle = 0.99 * 10^(-Compression / 20)

Features per step: ln of the peak detector level, ln of the smallest fast ratio, ln of the analytical slow ratio,
ln M. Loss: the null residual itself, 10 log10(sum (y - ref)^2 / sum ref^2) per example, averaged.
Runs in the PyTorch environment (C:\\Users\\wint1\\.venvs\\wavy-torch).

    python train_wvox.py [steps] [hidden]
"""
import sys, os, glob, math, time
import numpy as np
import torch

FS = 48000
LAT = 64
D = 4                     # samples per GRU step at 48 kHz
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "measurements", "rvox", "train")
OUTD = os.path.join(HERE, "..", "build", "wvoxnet")
os.makedirs(OUTD, exist_ok=True)
dev = torch.device("cuda")
MODE = os.environ.get("MODE", "sup")        # sup: the reference gain as target; null: the null residual itself
SKIP = FS // 10


HOLDS = (12, 30, 60, 120, 240)    # steps: 1, 2.5, 5, 10, 20 ms of history for the running minimum of the fast ratio
PEAKS = (120, 360, 1200, 3600)    # steps: 10, 30, 100, 300 ms of history for the running maximum of the level
BASE = os.environ.get("BASE", "hold")   # hold: S = (static law at the loudest level of the last 50 ms) * exp(c >= 0)
                                        # analytical: S = (the fitted analytical slow ratio) * exp(c)
WBASE = 600                             # steps (50 ms) of the base's peak hold
FLAT = float(os.environ.get("FLAT", "0.001"))  # steady guard: when ln F varied less than this over the last 240 steps
                                               # (20 ms) the correction fades out over 240 steps (0: no guard)
NFEAT = 4 + len(HOLDS) + len(PEAKS) + (BASE == "hold")
FLOOR = int(os.environ.get("FLOOR", "12000"))   # steps (1 s): the slow ratio never goes below the static-law ratio at
                                                # the loudest level of this long (0: no floor). A steady signal then
                                                # keeps exactly the fast ratio, whatever the network says.


def running_max(a, w):
    from scipy.ndimage import maximum_filter1d
    return maximum_filter1d(a, w, origin=(w - 1) // 2, mode="nearest")


def running_min(a, w):
    """minimum over the last w entries (causal)"""
    from scipy.ndimage import minimum_filter1d
    return minimum_filter1d(a, w, origin=(w - 1) // 2, mode="nearest")


def features(v, fast, slow, comp):
    """per-step features from the processed-sample arrays (length a multiple of D)"""
    n = len(v) // D
    vb = np.maximum(v[:n * D].reshape(n, D).max(1), 1e-5)
    fb = np.log(np.maximum(fast[:n * D].reshape(n, D).min(1), 1e-6))
    sb = np.maximum(slow[:n * D].reshape(n, D)[:, -1], 1e-6)
    M = 10 ** (-comp / 20)
    lv = np.log(vb)
    cols = [lv / 5, fb / 2, np.log(sb) / 2, np.full(n, math.log(M) / 4)] + [running_min(fb, w) / 2 for w in HOLDS] \
        + [running_max(lv, w) / 5 for w in PEAKS] + ([hold_base_steps(vb, M) / 2] if BASE == "hold" else [])
    return np.stack(cols, 1).astype(np.float32)


def hold_base_steps(vb, M):
    """ln of the static-law ratio at the loudest step level of the last WBASE steps (this step included)"""
    return -np.log1p((M - 1) * running_max(vb, WBASE))


def prepare(v, fast, slow, comp):
    """features per step, and per processed sample ln F and ln S_base (the base the network's correction acts on)"""
    T = (len(v) // D) * D; n = T // D
    M = 10 ** (-comp / 20)
    fe = features(v, fast, slow, comp)
    lnF = np.log(np.maximum(fast[:T], 1e-6)).astype(np.float32)
    if BASE == "hold":
        vb = np.maximum(v[:T].reshape(n, D).max(1), 1e-5)
        hb = hold_base_steps(vb, M)
        prev = np.concatenate([[0.0], hb[:-1]])             # samples of step j use the base finished at step j-1
        lnS = np.repeat(prev, D).astype(np.float32)
    else:
        lnS = np.log(np.maximum(slow[:T], 1e-6)).astype(np.float32)
    r = np.ones(n, np.float32)
    if FLAT > 0:
        lfs = np.log(np.maximum(fast[:T].reshape(n, D).min(1), 1e-6))
        flat = (running_max(lfs, 240) - running_min(lfs, 240)) < FLAT
        g = 1.0
        for j in range(n):
            g = max(0.0, g - 1 / 240) if flat[j] else min(1.0, g + 1 / 240)
            r[j] = g
    fe = np.concatenate([fe, r[:, None]], 1)
    lnB = np.full(T, -50.0, np.float32)
    if FLOOR > 0:
        vb = np.maximum(v[:T].reshape(n, D).max(1), 1e-5)
        fl = -np.log1p((M - 1) * running_max(vb, FLOOR))
        lnB = np.repeat(np.concatenate([[0.0], fl[:-1]]), D).astype(np.float32)
    return fe, lnF, np.stack([lnS, lnB])


def load(pattern):
    ex = []
    for f in sorted(glob.glob(os.path.join(DATA, pattern + "*.npz"))):
        z = np.load(f)
        T = (len(z["v"]) // D) * D
        comp = float(z["comp"]); idle = 0.99 * 10 ** (-comp / 20)
        xd = np.zeros(T, np.float32); xd[LAT:] = z["x"][:T - LAT] * idle      # input as it leaves the delay, times idle
        rd = np.zeros(T, np.float32); rd[LAT:] = z["ref"][:T - LAT]
        fe, lnF, lnS = prepare(z["v"], z["fast"], z["slow"], comp)
        ex.append(dict(name=os.path.basename(f), kind=str(z["kind"]), comp=comp, fe=fe, lnF=lnF, lnS=lnS, xd=xd, ref=rd))
    return ex


def upsample(c):
    """step outputs (B, n) -> per sample (B, n*D): sample m of step j uses c[j-2] -> c[j-1] (finished steps only)"""
    B, n = c.shape
    pad = torch.cat([torch.zeros(B, 2, device=c.device), c[:, :-2]], 1)       # c[j-2]
    nxt = torch.cat([torch.zeros(B, 1, device=c.device), c[:, :-1]], 1)       # c[j-1]
    w = (torch.arange(D, device=c.device, dtype=torch.float32) + 1) / D
    return (pad[:, :, None] + (nxt - pad)[:, :, None] * w).reshape(B, n * D)


class Net(torch.nn.Module):
    def __init__(self, hidden=32):
        super().__init__()
        self.gru = torch.nn.GRU(NFEAT, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, 1)
        torch.nn.init.zeros_(self.out.weight); torch.nn.init.zeros_(self.out.bias)   # start at the analytical model
        if BASE == "hold": torch.nn.init.constant_(self.out.bias, -4.0)             # start just above the hold base

    def forward(self, fe, lnF, lnS, xd, temp):
        hs, h = [], None
        gate, fe = fe[..., -1], fe[..., :-1]
        for c in range(0, fe.shape[1], 8192):
            y, h = self.gru(fe[:, c:c + 8192].contiguous(), h)
            hs.append(y)
        o = self.out(torch.cat(hs, 1))[..., 0]
        corr = upsample((torch.nn.functional.softplus(o) if BASE == "hold" else o) * gate)
        lnSc = torch.maximum(lnS[:, 0] + corr, lnS[:, 1])        # [:, 1]: the floor
        lng = torch.minimum(lnF, lnSc)
        if temp > 0:      # forward: the exact min; backward: the soft min's gradient, so S feels F nearby
            soft = -temp * torch.logsumexp(torch.stack([-lnF / temp, -lnSc / temp]), 0)
            lng = lng + (soft - soft.detach())
        self.lnSc = lnSc
        return xd * torch.exp(lng), corr


def batch(exs, idx):
    g = lambda k: torch.tensor(np.stack([exs[i][k] for i in idx])).to(dev)
    return g("fe"), g("lnF"), g("lnS"), g("xd"), g("ref")


def supervised(lnSc, lnF, xd, ref, eps=0.003):
    """the reference's own gain read sample by sample (ref / delayed input): where it is below the fast ratio the slow
    ratio must equal it; elsewhere the slow ratio must not go below the fast ratio. Weighted by the signal energy."""
    w = xd ** 2
    ok = (w > 1e-10)
    lng = torch.log(torch.clamp(torch.abs(ref) / torch.clamp(torch.abs(xd), min=1e-9), 1e-6, 10.0))
    act = ok & (lng < lnF - eps)
    e = torch.where(act, lnSc - lng, torch.relu(lnF - lnSc))
    w = w * ok
    w[:, :SKIP] = 0
    return ((w * e ** 2).sum(1) / (w.sum(1) + 1e-12))


def loss_db(y, t):
    num = ((y - t)[:, SKIP:] ** 2).sum(1); den = (t[:, SKIP:] ** 2).sum(1) + 1e-12
    return 10 * torch.log10(num / den + 1e-12)


def evaluate(net, exs, temp=0.0, bs=8):
    net.eval(); out = []
    with torch.no_grad():
        for s in range(0, len(exs), bs):
            idx = list(range(s, min(len(exs), s + bs)))
            fe, lnF, lnS, xd, ref = batch(exs, idx)
            y, _ = net(fe, lnF, lnS, xd, temp)
            out += loss_db(y, ref).cpu().tolist()
    net.train()
    return np.array(out)


if __name__ == "__main__":
    steps = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    hidden = int(sys.argv[2]) if len(sys.argv) > 2 else 32
    torch.manual_seed(0); np.random.seed(0)
    tr = load("train") + (load("more") if os.environ.get("MORE") else []); va = load("val")
    print("train %d, val %d examples" % (len(tr), len(va)), flush=True)
    net = Net(hidden).to(dev)
    init = os.environ.get("INIT")              # continue from a saved model (e.g. after supervised pre-training)
    if init: net.load_state_dict(torch.load(init))
    tag = os.environ.get("TAG", "")
    v0 = evaluate(net, va)
    print("start (analytical slow path): val median %.1f dB, mean %.1f" % (np.median(v0), v0.mean()), flush=True)
    lr = float(os.environ.get("LR", "5e-4"))
    opt = torch.optim.Adam(net.parameters(), lr=lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, steps)
    best = np.median(v0); torch.save(net.state_dict(), os.path.join(OUTD, "wvoxnet_h%d%s.pt" % (hidden, tag)))
    t0 = time.time(); bs = int(os.environ.get("BS", "12"))
    temp0 = float(os.environ.get("TEMP0", "0.03"))
    # BALANCE=1: draw examples so that every Compression setting is equally likely (the renders favour heavy settings)
    comps = np.array([e["comp"] for e in tr])
    if os.environ.get("BALANCE"):
        hist, edges = np.histogram(comps, bins=12, range=(-36, 0))
        p = 1.0 / np.maximum(hist[np.clip(np.digitize(comps, edges) - 1, 0, 11)], 1); p /= p.sum()
    else:
        p = None
    for it in range(1, steps + 1):
        temp = temp0 * (0.005 / temp0) ** min(1.0, it / steps)            # gradient width (ln units) -> 0.04 dB
        idx = np.random.choice(len(tr), bs, replace=False, p=p)
        fe, lnF, lnS, xd, ref = batch(tr, idx)
        y, corr = net(fe, lnF, lnS, xd, temp)
        loss = supervised(net.lnSc, lnF, xd, ref).mean() if MODE == "sup" else loss_db(y, ref).mean()
        opt.zero_grad(); loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0)
        opt.step(); sched.step()
        if it % 100 == 0 or it == steps:
            v = evaluate(net, va)
            print("step %5d  temp %.3f  train %.1f dB  val median %.1f dB (mean %.1f, worst %.1f)  %.0f s" % (
                it, temp, loss.item(), np.median(v), v.mean(), v.max(), time.time() - t0), flush=True)
            torch.save(net.state_dict(), os.path.join(OUTD, "wvoxnet_h%d%s_last.pt" % (hidden, tag)))
            if np.median(v) < best:
                best = np.median(v); torch.save(net.state_dict(), os.path.join(OUTD, "wvoxnet_h%d%s.pt" % (hidden, tag)))
    vl = evaluate(net, va)
    net.load_state_dict(torch.load(os.path.join(OUTD, "wvoxnet_h%d%s.pt" % (hidden, tag))))
    v = evaluate(net, va)
    kinds = {}
    for e, d, d0, dl in zip(va, v, v0, vl): kinds.setdefault(e["kind"], []).append((d0, d, dl))
    print("val median: start %.1f, best %.1f, last %.1f dB; by kind (start / best / last):" % (np.median(v0), np.median(v), np.median(vl)))
    for k, a in sorted(kinds.items()):
        a = np.array(a); print("  %-12s %6.1f  %6.1f  %6.1f  (n=%d)" % (k, *np.median(a, 0), len(a)))
