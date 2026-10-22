"""WComp Phase 4: train H-Comp's release law end to end with exact gradients (PyTorch), on the probes exported to
build/hc_torch_set.npz (ident_hc_rel4.py / ident_hc_rel6.py cases).

Model (as hcfast6.c): R rises to the target at once and falls at F(R) / (release * w) + A(R) dB/s, never below it;
ln w = ln w1(R - S1) + m(R / 20, (R - S1) / 10, (R - S2) / 10, (R - S3) / 10, ln(release / 100 ms) / 2), S1..S3 averages
of R (one-pole on the linear gain; tau1 learned, 30 and 5 ms), all S at S0 after a reset; then the attack one-pole
(falling only) and the two moving averages (38, 27). F, A, w1 are log tables on knots, m a small MLP.

    <venv python> train_hc_release.py [iterations] [batch]        writes measurements/hcomp/release_law6.json
"""
import sys, os, json, time
import numpy as np
import torch

torch.set_num_threads(8)
HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, '..', 'build')
OUT = os.path.join(HERE, '..', 'measurements', 'hcomp', 'release_law6.json')
FS = 48000.0
FX = [0, 0.5, 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 25, 30, 40, 50]
AX = [0, 0.5, 1, 1.5, 2, 3, 5]
WX = [0, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 20, 30]
NH = 8
DT = torch.float32
DEV = torch.device('cuda' if torch.cuda.is_available() and os.environ.get('HC_CPU') is None else 'cpu')


def interp(x, knots, logv):
    """exp of the piecewise-linear table, clamped at the ends."""
    k = knots
    xc = torch.clamp(x, k[0], k[-1])
    i = torch.clamp(torch.bucketize(xc, k) - 1, 0, len(k) - 2)
    x0, x1 = k[i], k[i + 1]
    f = (xc - x0) / (x1 - x0)
    return torch.exp(logv[i] + f * (logv[i + 1] - logv[i]))


class Law(torch.nn.Module):
    def __init__(self, base):
        super().__init__()
        nF, nA, nW = len(FX), len(AX), len(WX)
        self.FX, self.AX, self.WX = [torch.tensor(k, dtype=DT, device=DEV) for k in (FX, AX, WX)]
        self.logF = torch.nn.Parameter(torch.tensor(base[:nF], dtype=DT))
        self.logA = torch.nn.Parameter(torch.tensor(base[nF:nF + nA], dtype=DT))
        self.logW = torch.nn.Parameter(torch.tensor(base[nF + nA:nF + nA + nW], dtype=DT))
        self.logTau = torch.nn.Parameter(torch.tensor(base[nF + nA + nW], dtype=DT))
        self.S0 = torch.nn.Parameter(torch.tensor(base[nF + nA + nW + 1], dtype=DT))
        self.l1 = torch.nn.Linear(5, NH)
        self.l2 = torch.nn.Linear(NH, 1)
        torch.nn.init.zeros_(self.l2.weight); torch.nn.init.zeros_(self.l2.bias)

    def forward(self, Rt, att, rel, startup):
        B, N = Rt.shape
        c1 = torch.exp(-1.0 / (torch.exp(self.logTau) * 1e-3 * FS))
        c2, c3 = float(np.exp(-1 / (30e-3 * FS))), float(np.exp(-1 / (5e-3 * FS)))
        ac = torch.exp(-1.0 / (att * 0.99e-3 * FS))
        rels = rel * 1e-3
        lr = torch.log(rel / 100.0) / 2.0
        R = torch.zeros(B, dtype=DT, device=DEV)
        S1 = startup * self.S0; S2 = S1.clone(); S3 = S1.clone()
        gS1, gS2, gS3 = [10 ** (-s / 20) for s in (S1, S2, S3)]
        g1 = torch.ones(B, dtype=DT, device=DEV)
        out = []
        for n in range(N):
            t = Rt[:, n]
            d1 = R - S1
            w1 = torch.where(d1 > 0, interp(torch.clamp(d1, min=0.0), self.WX, self.logW), torch.exp(self.logW[0]))
            feat = torch.stack([R / 20, d1 / 10, (R - S2) / 10, (R - S3) / 10, lr], 1)
            m = self.l2(torch.tanh(self.l1(feat)))[:, 0]
            rate = interp(R, self.FX, self.logF) / (rels * w1 * torch.exp(m)) + interp(R, self.AX, self.logA)
            R = torch.maximum(torch.clamp(R - rate / FS, min=0.0), t)
            gR = 10 ** (-R / 20)
            gS1 = gR + c1 * (gS1 - gR); gS2 = gR + c2 * (gS2 - gR); gS3 = gR + c3 * (gS3 - gR)
            S1, S2, S3 = [-20 * torch.log10(gs) for gs in (gS1, gS2, gS3)]
            g1 = torch.where(gR < g1, gR + ac * (g1 - gR), gR)
            out.append(g1)
        g1 = torch.stack(out, 1)
        return box(box(g1, 38), 27)


def box(x, n):
    pad = torch.cat([torch.ones(x.shape[0], n, dtype=DT, device=x.device), x], 1)
    c = torch.cumsum(pad, 1)
    return (c[:, n:] - c[:, :-n]) / n


def main():
    iters = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    batch = int(sys.argv[2]) if len(sys.argv) > 2 else 96
    d = np.load(os.path.join(BUILD, 'hc_torch_set.npz'))
    Rt, G, M, meta = [torch.tensor(d[k], dtype=DT, device=DEV) for k in ('Rt', 'G', 'M', 'meta')]
    if os.path.exists(OUT):
        j = json.load(open(OUT)); base = j['base']; state = j.get('state')
    else:
        base = json.load(open(os.path.join(BUILD, 'release_law4.avg.json')))['q']; state = None
    law = Law(base).to(DEV)
    if state: law.load_state_dict({k: torch.tensor(v, dtype=DT, device=DEV) for k, v in state.items()})
    opt = torch.optim.Adam(law.parameters(), lr=3e-3)
    Gdb = 20 * torch.log10(torch.clamp(G, min=1e-6))
    rng = np.random.default_rng(1)
    dec = 4            # the loss on every 4th sample (the boxes make neighbours nearly equal)
    def loss_of(idx):
        y = law(Rt[idx], meta[idx, 0], meta[idx, 1], meta[idx, 2])
        e = (20 * torch.log10(torch.clamp(y, min=1e-6)) - Gdb[idx]) * M[idx]
        return (e[:, ::dec] ** 2).sum() / M[idx][:, ::dec].sum()
    t0 = time.time()
    for it in range(iters):
        idx = torch.tensor(rng.choice(len(Rt), batch, replace=False), device=DEV)
        opt.zero_grad()
        L = loss_of(idx)
        L.backward()
        opt.step()
        if it % 5 == 0: print('it %d loss %.4f (rms %.3f dB) %.0f s' % (it, L.item(), L.item() ** 0.5, time.time() - t0), flush=True)
        if it % 25 == 24 or it == iters - 1:
            with torch.no_grad():
                st = {k: v.tolist() for k, v in law.state_dict().items()}
            json.dump({'base': base, 'state': st, 'FX': FX, 'AX': AX, 'WX': WX, 'nh': NH, 'fast_ms': [30.0, 5.0]}, open(OUT, 'w'))


if __name__ == '__main__':
    main()
