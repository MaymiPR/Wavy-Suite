"""Trains WComp's learned gain correction (run in the PyTorch environment, C:\\Users\\wint1\\Desktop\\AI Projects\\Plugins\\.venv-gpu):
a GRU over WComp's own states and settings (render_trainset_wcomp.py FEATURES, one step per 16 samples) gives a
correction in dB of the compressor's gain, held over each step and faded out without gain reduction (guard) and while
the target reduction stays flat (flat_guard: steady tones, which the analytical model nulls already). The loss
is the null itself: the mean over cases of 10 log10(energy(WComp x correction - H-Comp) / energy(H-Comp) + 1e-7) from
0.3 s for the weak cases (worse than -35 dB without the net), plus a penalty on every case that ends up more than 0.5 dB worse
than its own null without the net (or than -60 dB, if it was better): the nulls that are good already must stay so.
The first epochs train on the weak cases alone; the penalty then comes in over 200 epochs.

    python nulltest/train_wcomp.py [hidden] [epochs] [tag]     -> build/wcompnet/wcompnet_<tag>.pt
"""
import sys, os, glob, time
import numpy as np
import torch
import torch.nn as nn

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = [os.path.join(HERE, '..', 'build', 'wcompnet', d) for d in os.environ.get('WCOMP_DATA', 'data').split(',')]   # the first holds the fixed cases
TEST = os.path.join(HERE, '..', 'build', 'wcompnet', 'test')
STEP, FS, SKIP = 16, 48000, int(0.3 * 48000)
HID = int(sys.argv[1]) if len(sys.argv) > 1 else 48
EPOCHS = int(sys.argv[2]) if len(sys.argv) > 2 else 300
TAG = sys.argv[3] if len(sys.argv) > 3 else 'h%d' % HID
dev = 'cuda'


class Net(nn.Module):
    def __init__(self, inputs=11, hidden=HID):
        super().__init__()
        self.gru = nn.GRU(inputs, hidden, batch_first=True)
        self.out = nn.Linear(hidden, 1)
        nn.init.zeros_(self.out.weight); nn.init.zeros_(self.out.bias)     # starts as no correction

    def forward(self, f):
        h, _ = self.gru(f)
        return 9.0 * torch.tanh(self.out(h)[..., 0] / 9.0)                  # dB, within +-9


def load(dirs=None):
    files = [f for d in (dirs or DATA) for f in sorted(glob.glob(os.path.join(d, 'case_*.npz')))]
    F, G, YW, YH, kind = [], [], [], [], []
    for i, fn in enumerate(files):
        d = np.load(fn)
        n = min(len(d['yW']), len(d['yH']), len(d['feat']) * STEP) // STEP * STEP
        F.append(d['feat'][:n // STEP]); G.append(d['guard'][:n // STEP]); YW.append(d['yW'][:n]); YH.append(d['yH'][:n])
        kind.append(int(os.path.basename(fn)[5:9]))
    L = min(len(y) for y in YW)
    t = lambda a, m: torch.tensor(np.stack([v[:m] for v in a]), dtype=torch.float32)
    F, G = t(F, L // STEP), t(G, L // STEP)
    return F, G * flat_guard(F), t(YW, L), t(YH, L), np.array(kind)


FLAT_STEPS, FLAT_DB, FLAT_RAMP = 60, 0.05, 0.15


def flat_guard(F):
    """0 while the target reduction stayed within FLAT_DB over the last 60 steps (20 ms): a steady tone, which the
    analytical model nulls already; full from FLAT_DB + FLAT_RAMP (WCompDsp.h GainNet uses the same rule)"""
    t = F[..., 1] * 24.0
    pad = torch.cat([t[:, :1].expand(-1, FLAT_STEPS - 1), t], 1)
    w = pad.unfold(1, FLAT_STEPS, 1)
    rng = w.max(-1).values - w.min(-1).values
    return ((rng - FLAT_DB) / FLAT_RAMP).clamp(0, 1)


def nulls(net, F, G, YW, YH, idx, bs=48):
    out = []
    with torch.no_grad():
        for i in range(0, len(idx), bs):
            j = idx[i:i + bs]
            out.append(case_null(net, F[j], G[j], YW[j], YH[j]))
    return torch.cat(out)


FLOOR, PENALTY, KEEP, WARM = 1e-7, float(os.environ.get('WCOMP_PENALTY', 1.0)), -60.0, 300     # KEEP: a case better than -60 dB may move within it


def case_null(net, f, g, yw, yh, floor=1e-12):
    d = net(f) * g if net is not None else torch.zeros_like(g)
    gain = torch.pow(10.0, d / 20.0).repeat_interleave(STEP, dim=1)
    e = ((yw * gain - yh)[:, SKIP:] ** 2).sum(1)
    ref = (yh[:, SKIP:] ** 2).sum(1).clamp_min(1e-9)
    return 10 * torch.log10(e / ref + floor)


def loss_of(net, f, g, yw, yh, base_floored, weak, pen=1.0):
    # the weak cases' nulls are the objective; the others only must not get worse (by more than 0.5 dB): on the log of a
    # tiny error a good case's gradient would otherwise drown the weak cases'
    v = case_null(net, f, g, yw, yh, FLOOR)
    w = weak.float()
    obj = (v * w).sum() / w.sum().clamp_min(1.0)
    return obj + pen * PENALTY * torch.relu(v - base_floored - 0.5).mean()


def main():
    F, G, YW, YH, kind = load()
    F, G, YW, YH = F.to(dev), G.to(dev), YW.to(dev), YH.to(dev)
    n = len(kind)
    n_fixed = 112 + 104                                  # the null test's and the presets' cases come first
    rng = np.random.default_rng(3)
    rand = np.arange(n_fixed, n); rng.shuffle(rand)
    val = np.sort(rand[:len(rand) // 10]); train = np.sort(np.r_[np.arange(n_fixed), rand[len(rand) // 10:]])
    print('%d cases: %d train, %d validation, %d hidden, %d epochs' % (n, len(train), len(val), HID, EPOCHS), flush=True)
    base = nulls(None, F, G, YW, YH, np.arange(n)).cpu().numpy()
    baseF = torch.tensor(np.maximum(base, KEEP), dtype=torch.float32, device=dev)   # better than KEEP counts as KEEP
    weak = base > -35
    weakT = torch.tensor(weak, device=dev)
    print('without the net: train %.2f  val %.2f  grid %.2f  presets %.2f  random %.2f dB' % (
        base[train].mean(), base[val].mean(), base[:112].mean(), base[112:216].mean(), base[216:].mean()), flush=True)
    net = Net().to(dev)
    opt = torch.optim.AdamW(net.parameters(), lr=3e-3, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, EPOCHS)
    bs = 24; best = 1e9; t0 = time.time()
    for ep in range(EPOCHS):
        net.train(); tot = 0.0
        pen = min(1.0, max(0.0, (ep - WARM) / 200.0))       # the weak cases alone first, then the penalty comes in
        perm = rng.permutation(train[weak[train]] if pen == 0.0 else train)
        for i in range(0, len(perm), bs):
            j = perm[i:i + bs]
            loss = loss_of(net, F[j], G[j], YW[j], YH[j], baseF[j], weakT[j], pen)
            if not torch.isfinite(loss): continue
            opt.zero_grad(); loss.backward(); nn.utils.clip_grad_norm_(net.parameters(), 5.0)
            if all(torch.isfinite(p.grad).all() for p in net.parameters()): opt.step()
            tot += loss.item() * len(j)
        sched.step()
        if ep % 10 == 9 or ep == EPOCHS - 1:
            net.eval(); v = nulls(net, F, G, YW, YH, np.arange(n)).cpu().numpy()
            ch = v - base
            vw = weak[val]
            print('ep %3d  weak cases: train %.2f (was %.2f)  val %.2f (was %.2f)  grid %.2f (was %.2f)  presets %.2f (was %.2f) | better / worse by 1 dB: %d / %d, worst %+.1f (base %.1f)  %.0f s' % (
                ep + 1, v[train][weak[train]].mean(), base[train][weak[train]].mean(), v[val][vw].mean(), base[val][vw].mean(),
                v[:112].mean(), base[:112].mean(), v[112:216].mean(), base[112:216].mean(), (ch < -1).sum(), (ch > 1).sum(),
                ch.max(), base[np.argmax(ch)], time.time() - t0), flush=True)
            score = v[val][vw].mean() + PENALTY * np.maximum(0, v[val] - np.maximum(base[val], KEEP) - 0.5).mean()
            if ep >= WARM + 200 and score < best:
                best = score
                torch.save(net.state_dict(), os.path.join(HERE, '..', 'build', 'wcompnet', 'wcompnet_%s.pt' % TAG))
    np.save(os.path.join(HERE, '..', 'build', 'wcompnet', 'nulls_%s.npy' % TAG), np.stack([base, v]))
    print('saved; best val score %.2f dB' % best)
    if glob.glob(os.path.join(TEST, 'case_*.npz')):               # the unseen test set, with the saved (best) net
        net.load_state_dict(torch.load(os.path.join(HERE, '..', 'build', 'wcompnet', 'wcompnet_%s.pt' % TAG)))
        Ft, Gt, Wt, Ht, _ = load([TEST]); Ft, Gt, Wt, Ht = Ft.to(dev), Gt.to(dev), Wt.to(dev), Ht.to(dev)
        b0 = nulls(None, Ft, Gt, Wt, Ht, np.arange(len(Ft))).cpu().numpy(); b1 = nulls(net, Ft, Gt, Wt, Ht, np.arange(len(Ft))).cpu().numpy()
        wk = b0 > -35
        print('TEST (%d unseen cases): weak %.2f -> %.2f dB, all %.2f -> %.2f, better / worse by 1 dB %d / %d, worst change %+.1f (base %.1f)' % (
            len(b0), b0[wk].mean(), b1[wk].mean(), b0.mean(), b1.mean(), (b1 - b0 < -1).sum(), (b1 - b0 > 1).sum(), (b1 - b0).max(), b0[np.argmax(b1 - b0)]))


if __name__ == '__main__':
    main()
