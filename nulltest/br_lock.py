"""The learned lock of WBass Rider's ride (brmodel.c, lock 1): when the gain stops following the law after a note's
start and holds, learned from the reference's riding on the bass corpus (bass_corpus.py, br_corpus_ref.py).

Labels: for each note the model starts where the reference rides too, the reference's lock moment is where our law
(clamped to the range) last meets the value the reference then holds, just before its gain goes flat. Inputs: the
model's own detector state every millisecond of the attack (brmodel.c writes them: the same numbers the plug-in
computes); the lock does not feed back into the detector, so they are exact for any lock. A note held at the range's
end, or one the reference does not ride, gives no label; one the reference never holds before the next note gives
only "not yet".

    python -I br_lock.py oracle             the corpus with the model locking at the labelled moments (the upper bound)
    python -I br_lock.py train [hidden]     train (torch), write measurements/wbassrider/lock_net.json
    python -I br_lock.py eval [thr]         the corpus with the learned lock against the rule
"""
import sys, os, json, ctypes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import brmodel as B, br_fit as F, bass_corpus as K, br_corpus_ref as CR, br_corpus_eval as E

HERE = os.path.dirname(os.path.abspath(__file__))
NET = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'lock_net.json')
FS = 48000
LIB = B._lib
NIN = LIB.br_lock_inputs()
LIB.br_set_lock_net.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.c_double]
LIB.br_set_lock_schedule.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int]
LIB.br_set_lock_dump.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.c_int]
_keep = []


def setting_kw(st, g, td):
    resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
    return dict(response=resp, sensitivity=sens, artifacts=float(st.get('artifacts', 50.0)), target=float(st.get('target', -14.0)),
                **dict(g, **F.setting_params(td, resp, sens)))


def dump(x, kw):
    """the lock's inputs for every attack millisecond: (output-frame sample, ms since the start, inputs)"""
    buf = np.zeros((200000, NIN + 2)); LIB.br_set_lock_dump(buf.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), len(buf))
    gm, tr = B.ride(x, trace=True, **kw)
    n = LIB.br_lock_dumped(); LIB.br_set_lock_dump(None, 0)
    d = buf[:n].copy(); d[:, 0] -= B.LOOKAHEAD[False]
    return gm, tr, d


def labels(x, gr, tr, kw):
    """[(start, next start, lock sample or None for never)] in the output frame"""
    L = np.clip(kw['target'] - tr[:, 0] + B.DEFAULTS['c0'], kw.get('range_min', -12.0), kw.get('range_max', 12.0))
    st = np.where((tr[:, 2].astype(int) & 8) > 0)[0]
    st = [s for j, s in enumerate(st) if j == 0 or s - st[j - 1] > 96]
    out = []
    for j, s in enumerate(st):
        s2 = st[j + 1] if j + 1 < len(st) else len(x)
        end = min(s2, s + 400 * 48, len(x) - 1500)
        if end - s < 30 * 48: continue
        # the reference rides here: it follows the law within 0.5 dB 15 .. 35 ms after the start (after its smoothing)
        if np.median(np.abs(gr[s + 720:s + 1680] - L[s + 720:s + 1680])) > 0.5: continue
        seg = gr[s:end:48]; dd = np.abs(np.diff(seg))
        flat = [q for q in range(len(dd) - 30) if np.all(dd[q:q + 30] < 0.004)]
        if not flat:
            out.append((s, s2, None)); continue
        p = s + 48 * flat[0]; h = gr[p]
        if abs(h) > 11.9: continue
        lo = np.where(np.abs(L[s:p + 480] - h) < 0.05)[0]
        sg = np.where(np.diff(np.sign(L[s:p + 480] - h)) != 0)[0]
        cand = np.r_[lo, sg]
        if not len(cand): continue
        out.append((s, s2, s + int(cand.max())))
    return out


def corpus_items(parts, sets, g, td):
    for k in parts:
        x, d = K.part(k)
        for s in sets:
            yield k, s, x, CR.load(k, s), setting_kw(CR.SETTINGS[s], g, td)


def run_with(x, kw, mode, sched=None, thr=0.0):
    """sched: [(start, lock or None)] in the output frame (the oracle, mode 2)"""
    if sched is not None:
        L = B.LOOKAHEAD[False]
        flat = [v for s, t in sched for v in (int(s) + L, -1 if t is None else int(t) + L)]
        a = (ctypes.c_int * len(flat))(*flat); _keep.append(a)
        LIB.br_set_lock_schedule(a, len(sched))
    gm = B.ride(x, **dict(kw, lock=float(mode), lock_thr=thr))
    LIB.br_set_lock_schedule(None, 0)
    return gm


def oracle(parts, sets):
    g, td = F.load_fit(); res = {}
    for k, s, x, gr, kw in corpus_items(parts, sets, g, td):
        ok = E.loud(x)
        _, tr = B.ride(x, trace=True, **kw)
        lab = labels(x, gr, tr, kw)
        sched = sorted((s0, t) for s0, _, t in lab)
        r0 = E.residual(x, B.ride(x, **kw), gr, ok); r1 = E.residual(x, run_with(x, kw, 2, sched), gr, ok)
        res.setdefault(s, []).append((r0, r1, len(lab), sum(t is None for _, _, t in lab)))
    for s, v in res.items():
        a = np.array(v)
        print('%-8s rule %6.1f  oracle lock %6.1f  (median residual; %d notes labelled, %d never held)' % (s, np.median(a[:, 0]), np.median(a[:, 1]), a[:, 2].sum(), a[:, 3].sum()))


def dataset(parts, sets):
    g, td = F.load_fit(); X, Y, W, G = [], [], [], []
    for k, s, x, gr, kw in corpus_items(parts, sets, g, td):
        gm, tr, d = dump(x, kw)
        lab = labels(x, gr, tr, kw)
        idx = {int(t): q for q, t in enumerate(d[:, 0])}      # (the first frame is 1 ms after the start)
        for (st, s2, lk) in lab:
            q0 = idx.get(int(st) + 48)
            if q0 is None or d[q0, 1] != 1: continue
            frames = []
            q = q0
            while q < len(d) and d[q, 0] < s2 and (q == q0 or d[q, 1] > d[q - 1, 1]):
                frames.append(q); q += 1
            if not frames: continue
            kl = None if lk is None else (lk - st) / 48.0
            for q in frames:
                t = d[q, 1]
                if kl is not None and t > kl + 2: break
                y = 0.0 if (kl is None or t < kl) else 1.0
                X.append(d[q, 2:]); Y.append(y); G.append(k)
                W.append(1.0 if y == 0 else 3.0)
    return np.array(X), np.array(Y), np.array(W), np.array(G)


def train(hidden=16, epochs=400):
    import torch
    sets = list(CR.SETTINGS)
    Xa, Ya, Wa, Ga = dataset(list(K.TRAIN) + list(K.HELD), sets)
    tr = np.isin(Ga, list(K.TRAIN)); te = ~tr
    print('frames: train %d (%d lock), held %d (%d lock)' % (tr.sum(), Ya[tr].sum(), te.sum(), Ya[te].sum()), flush=True)
    torch.manual_seed(0)
    net = torch.nn.Sequential(torch.nn.Linear(NIN, hidden), torch.nn.Tanh(), torch.nn.Linear(hidden, 1))
    opt = torch.optim.Adam(net.parameters(), lr=3e-3)
    Xt, Yt, Wt = [torch.tensor(a[tr], dtype=torch.float32) for a in (Xa, Ya, Wa)]
    Xh, Yh, Wh = [torch.tensor(a[te], dtype=torch.float32) for a in (Xa, Ya, Wa)]
    lossf = torch.nn.BCEWithLogitsLoss(reduction='none')
    best = (1e9, None)
    for ep in range(epochs):
        perm = torch.randperm(len(Xt))
        for b in range(0, len(Xt), 4096):
            i = perm[b:b + 4096]
            opt.zero_grad(); l = (lossf(net(Xt[i]).squeeze(1), Yt[i]) * Wt[i]).mean(); l.backward(); opt.step()
        with torch.no_grad():
            lh = float((lossf(net(Xh).squeeze(1), Yh) * Wh).mean()); lt = float((lossf(net(Xt).squeeze(1), Yt) * Wt).mean())
        if lh < best[0]: best = (lh, {k: v.clone() for k, v in net.state_dict().items()})
        if ep % 25 == 0: print('epoch %3d  train %.4f  held %.4f' % (ep, lt, lh), flush=True)
    net.load_state_dict(best[1])
    sd = net.state_dict()
    json.dump(dict(hidden=hidden, w1=sd['0.weight'].double().flatten().tolist(), b1=sd['0.bias'].double().tolist(),
                   w2=sd['2.weight'].double().flatten().tolist(), b2=float(sd['2.bias'][0]), held_loss=best[0]), open(NET, 'w'))
    print('wrote', NET, 'held loss %.4f' % best[0])


def load_net(path=NET):
    d = json.load(open(path))
    arr = lambda v: (ctypes.c_double * len(v))(*v)
    a = [arr(d['w1']), arr(d['b1']), arr(d['w2'])]; _keep.append(a)
    LIB.br_set_lock_net(d['hidden'], a[0], a[1], a[2], d['b2'])
    return d


def evaluate(parts, sets, thr=0.0):
    g, td = F.load_fit(); load_net(); res = {}
    for k, s, x, gr, kw in corpus_items(parts, sets, g, td):
        ok = E.loud(x)
        mode = json.load(open(NET)).get('mode', 1)
        res.setdefault(s, []).append((E.residual(x, B.ride(x, **kw), gr, ok), E.residual(x, run_with(x, kw, mode, thr=thr), gr, ok)))
    for s, v in res.items():
        a = np.array(v)
        print('%-8s rule %6.1f (worst %6.1f)  learned %6.1f (worst %6.1f)' % (s, np.median(a[:, 0]), a[:, 0].max(), np.median(a[:, 1]), a[:, 1].max()), flush=True)


def dataset_follow(parts, sets, step=2):
    """lock 3: every other millisecond of every note (up to a second): does the reference follow the law (its gain
    within 0.15 dB of our smoothed law, both moving) or hold (its gain flat while our law moves)? frames where the
    law is flat say nothing and are left out"""
    g, td = F.load_fit(); X, Y, G = [], [], []
    a = np.exp(-1.0 / (0.005 * FS))
    for k, s, x, gr, kw in corpus_items(parts, sets, g, td):
        gm, tr, d = dump(x, dict(kw, lock=3.0))
        L = np.clip(kw['target'] - tr[:, 0] + B.DEFAULTS['c0'], -12.0, 12.0)
        from scipy.signal import lfilter
        Ls = lfilter([1 - a], [1, -a], L)
        ok = E.loud(x)
        for q in range(0, len(d), step):
            t = int(d[q, 0])
            if t < 240 or t > len(x) - 240 or not ok[t]: continue
            sl = abs(Ls[t + 48] - Ls[t - 48]); sr = abs(gr[t + 48] - gr[t - 48])
            if sl < 0.02: continue
            if abs(gr[t] - Ls[t]) < 0.15 and sr > 0.01: y = 1.0
            elif sr < 0.003: y = 0.0
            else: continue
            X.append(d[q, 2:]); Y.append(y); G.append(k)
    return np.array(X), np.array(Y), np.array(G)


def train_follow(hidden=16, epochs=300):
    import torch
    Xa, Ya, Ga = dataset_follow(list(K.TRAIN) + list(K.HELD), list(CR.SETTINGS))
    tr = np.isin(Ga, list(K.TRAIN)); te = ~tr
    print('frames: train %d (%.0f %% follow), held %d' % (tr.sum(), 100 * Ya[tr].mean(), te.sum()), flush=True)
    torch.manual_seed(0)
    net = torch.nn.Sequential(torch.nn.Linear(NIN, hidden), torch.nn.Tanh(), torch.nn.Linear(hidden, 1))
    opt = torch.optim.Adam(net.parameters(), lr=3e-3)
    Xt, Yt = [torch.tensor(v[tr], dtype=torch.float32) for v in (Xa, Ya)]
    Xh, Yh = [torch.tensor(v[te], dtype=torch.float32) for v in (Xa, Ya)]
    lossf = torch.nn.BCEWithLogitsLoss()
    best = (1e9, None)
    for ep in range(epochs):
        perm = torch.randperm(len(Xt))
        for b in range(0, len(Xt), 8192):
            i = perm[b:b + 8192]; opt.zero_grad(); l = lossf(net(Xt[i]).squeeze(1), Yt[i]); l.backward(); opt.step()
        with torch.no_grad():
            zh = net(Xh).squeeze(1); lh = float(lossf(zh, Yh)); acc = float(((zh > 0).float() == Yh).float().mean())
        if lh < best[0]: best = (lh, {kk: v.clone() for kk, v in net.state_dict().items()}, acc)
        if ep % 25 == 0: print('epoch %3d  held loss %.4f  accuracy %.3f' % (ep, lh, acc), flush=True)
    sd = best[1]
    # the rule alone on the same frames (its input 16: following)
    print('held accuracy: learned %.3f, the rule %.3f' % (best[2], float(np.mean((Xa[te][:, 16] > 0.5) == (Ya[te] > 0.5)))))
    json.dump(dict(hidden=hidden, mode=3, w1=sd['0.weight'].double().flatten().tolist(), b1=sd['0.bias'].double().tolist(),
                   w2=sd['2.weight'].double().flatten().tolist(), b2=float(sd['2.bias'][0]), held_loss=best[0]), open(NET, 'w'))
    print('wrote', NET)


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'oracle'
    if cmd == 'oracle':
        which = sys.argv[2] if len(sys.argv) > 2 else 'train'
        oracle(list(K.TRAIN) if which == 'train' else list(K.HELD), list(CR.SETTINGS))
    elif cmd == 'train': train(int(sys.argv[2]) if len(sys.argv) > 2 else 16)
    elif cmd == 'train3': train_follow(int(sys.argv[2]) if len(sys.argv) > 2 else 16)
    elif cmd == 'eval':
        thr = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
        for which in ('train', 'held'):
            print('--', which); evaluate(list(K.TRAIN) if which == 'train' else list(K.HELD), list(CR.SETTINGS), thr)
