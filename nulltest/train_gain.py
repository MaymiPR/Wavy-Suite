"""Learned gain control for WBass (gray-box): the identified structure of the generated path stays fixed and exact,
only the level-to-gain block is learned.

    b = LP3(F) x,  l = LP3(F/3) x,  u = HP3(F) l                       (fixed, precomputed)
    [g_pre, g_post] = exp(Linear(GRU(features)))                       (learned; features: compressed |x|, |b|, |l|, |u|,
                                                                        log F; magnitudes only, so the gain is symmetric
                                                                        and Decay 0 leaves no even harmonics, as in the
                                                                        reference)
    n = C0 g_post HP3(F)(g_pre l)
    q = HP3(F/3)(n * C0 g_pre g_post * b)
    generated = HP3(F)(-n + k2 Decay q)

Targets: the reference's generated part (render_trainset.py). Loss: mean over examples of the residual in dB.
Runs in the PyTorch environment (C:\\Users\\wint1\\.venvs\\wavy-torch).

    python train_gain.py [steps] [hidden]
"""
import sys, os, glob, json, math, time
import numpy as np
from scipy.signal import butter, sosfilt, sosfilt_zi
import torch

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "measurements", "rbass", "train")
OUTD = os.path.join(HERE, "..", "build", "gainnet")
os.makedirs(OUTD, exist_ok=True)
dev = torch.device("cuda")
IRLEN = 1 << 15
SKIP = FS // 10


def sos(o, k, fc): return butter(o, fc, btype=k, fs=FS, output='sos')


def ir(s, n=IRLEN):
    d = np.zeros(n); d[0] = 1.0
    return sosfilt(s, d)


def feat(v): return np.abs(np.arcsinh(v / 1e-3)) / 8.0     # magnitudes only: the reference's gain is symmetric


_irs = {}
def irs(F):
    if F not in _irs:
        _irs[F] = (ir(sos(3, 'high', F)), ir(sos(3, 'high', F / 3)))
    return _irs[F]


def load(split):
    ex = []
    for f in sorted(glob.glob(os.path.join(DATA, split + "*.npz"))):
        z = np.load(f); x = z["x"].astype(np.float64); F = float(z["F"])
        b = sosfilt(sos(3, 'low', F), x); l = sosfilt(sos(3, 'low', F / 3), x); u = sosfilt(sos(3, 'high', F), l)
        fe = np.stack([feat(x), feat(b), feat(l), feat(u), np.full_like(x, math.log(F / 80.0))], 1).astype(np.float32)
        ex.append(dict(name=os.path.basename(f), F=F, decay=float(z["decay"]), kind=str(z["kind"]), fe=fe,
                       l=l.astype(np.float32), b=b.astype(np.float32), gen=z["gen"].astype(np.float32)))
    return ex


def fftconv(sig, kern):
    """causal convolution along the last axis; sig (B, N), kern (B, K)"""
    n = sig.shape[-1] + kern.shape[-1]
    L = 1 << (n - 1).bit_length()
    y = torch.fft.irfft(torch.fft.rfft(sig, L) * torch.fft.rfft(kern, L), L)
    return y[..., :sig.shape[-1]]


class Net(torch.nn.Module):
    def __init__(self, hidden=24):
        super().__init__()
        self.gru = torch.nn.GRU(5, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, 2)
        torch.nn.init.zeros_(self.out.weight); torch.nn.init.zeros_(self.out.bias)   # start at gain 1: the linear path
        self.logC0 = torch.nn.Parameter(torch.tensor(math.log(298.0)))
        self.k2 = torch.nn.Parameter(torch.tensor(-0.0078))

    def forward(self, fe, l, b, decay, hpF, hpT):
        hs, h = [], None
        for c in range(0, fe.shape[1], 8192):          # cuDNN limits the sequence length per call; carry the state
            y, h = self.gru(fe[:, c:c + 8192].contiguous(), h)
            hs.append(y)
        o = self.out(torch.cat(hs, 1))
        gpre, gpost = torch.exp(o[..., 0]), torch.exp(o[..., 1])
        C0 = torch.exp(self.logC0)
        n = C0 * gpost * fftconv(gpre * l, hpF)
        q = fftconv(n * C0 * gpre * gpost * b, hpT)
        return fftconv(-n + self.k2 * decay[:, None] * q, hpF), gpre, gpost


def batch(exs, idx):
    g = lambda k: torch.tensor(np.stack([exs[i][k] for i in idx])).to(dev)
    hpF = torch.tensor(np.stack([irs(exs[i]["F"])[0] for i in idx]), dtype=torch.float32).to(dev)
    hpT = torch.tensor(np.stack([irs(exs[i]["F"])[1] for i in idx]), dtype=torch.float32).to(dev)
    dec = torch.tensor([exs[i]["decay"] for i in idx], dtype=torch.float32).to(dev)
    return g("fe"), g("l"), g("b"), dec, hpF, hpT, g("gen")


def loss_db(y, t):
    num = ((y - t)[:, SKIP:] ** 2).sum(1); den = (t[:, SKIP:] ** 2).sum(1) + 1e-12
    return 10 * torch.log10(num / den + 1e-9)


def evaluate(net, exs, bs=8):
    net.eval(); out = []
    with torch.no_grad():
        for s in range(0, len(exs), bs):
            idx = list(range(s, min(len(exs), s + bs)))
            fe, l, b, dec, hpF, hpT, gen = batch(exs, idx)
            y, _, _ = net(fe, l, b, dec, hpF, hpT)
            out += loss_db(y, gen).cpu().tolist()
    net.train()
    return np.array(out)


if __name__ == "__main__":
    steps = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    hidden = int(sys.argv[2]) if len(sys.argv) > 2 else 24
    torch.manual_seed(0)
    tr = load("train"); va = load("val")
    print("train %d, val %d examples" % (len(tr), len(va)), flush=True)
    net = Net(hidden).to(dev)
    opt = torch.optim.Adam(net.parameters(), lr=2e-3)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, steps)
    v0 = evaluate(net, va); print("start: val median %.1f dB" % np.median(v0), flush=True)
    best = 1e9; t0 = time.time(); bs = 12
    for it in range(1, steps + 1):
        idx = np.random.choice(len(tr), bs, replace=False)
        fe, l, b, dec, hpF, hpT, gen = batch(tr, idx)
        y, gpre, gpost = net(fe, l, b, dec, hpF, hpT)
        loss = loss_db(y, gen).mean()
        opt.zero_grad(); loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0)
        opt.step(); sched.step()
        if it % 100 == 0 or it == steps:
            v = evaluate(net, va)
            print("step %5d  train %.1f dB  val median %.1f dB (mean %.1f, worst %.1f)  C0 %.1f k2 %.5f  %.0f s" % (
                it, loss.item(), np.median(v), v.mean(), v.max(), math.exp(net.logC0.item()), net.k2.item(), time.time() - t0), flush=True)
            if np.median(v) < best:
                best = np.median(v); torch.save(net.state_dict(), os.path.join(OUTD, "gainnet_h%d.pt" % hidden))
    net.load_state_dict(torch.load(os.path.join(OUTD, "gainnet_h%d.pt" % hidden)))
    v = evaluate(net, va)
    kinds = {}
    for e, d in zip(va, v): kinds.setdefault(e["kind"], []).append(d)
    print("best val median %.1f dB; by kind:" % np.median(v), {k: round(float(np.median(x)), 1) for k, x in kinds.items()})
