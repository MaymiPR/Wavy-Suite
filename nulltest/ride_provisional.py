"""A provisional riding model for WBass Rider's editor (Phase 2), fitted on films of the reference (MeterCapture gl=1
with steady tones and a bass line; nulltest/read_br_film.py): what the Energy meter shows and where the Rider sits,
frame by frame. Not the reference's DSP (Phase 3 / 4 identify that); only what the editor shows, close enough that
WBass Rider responds to audio as Bass Rider does.

Model (per sample, 48 kHz):
  w      = first-order high-pass (fc) of the input (the energy weighting)
  fast   = peak follower of |w|: instant rise, one-pole release (relF)
  slow   = one-pole of fast, attack attS, release relS
  energy = 20 log10 (fast) + off                      (the Energy meter, before its display ballistics)
  a note begins when energy > spill gate and fast exceeds slow by onDb (or after the idle state)
  during the first settle s of a note the gain follows target - energy(slow) (the Rider's overshoot), then holds
  a note ends when fast falls below the note's peak - offDb or the gate: after holdMs the Rider glides to idle (tau)
  meter display: instant rise, fall at most fallDb per second

    python -I ride_provisional.py [fit]
"""
import sys, os
import numpy as np
from scipy import signal, optimize

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, '..', 'reference', 'wbassrider')
FS = 48000.0
P0 = dict(fc=115.0, relF=0.060, attS=0.030, relS=1.0, off=3.0, onDb=6.0, settle=0.10, offDb=24.0, holdMs=60.0, tau=0.040,
          fallDb=43.0, gate=-40.0)


def simulate(x, p, target=-14.0, lo=-12.0, hi=12.0, idle=0.0, frame_t=None):
    b, a = signal.butter(1, p['fc'] / (FS / 2), 'high')
    w = np.abs(signal.lfilter(b, a, x))
    n = len(w)
    # decimate the control to 1 kHz (the editor's needs) with a max over each ms
    step = 48
    m = n // step
    pk = w[:m * step].reshape(m, step).max(1)
    dt = step / FS
    kF = np.exp(-dt / p['relF']); kA = np.exp(-dt / p['attS']); kR = np.exp(-dt / p['relS'])
    fast = slow = 1e-6
    E = np.zeros(m); G = np.zeros(m)
    gain = idle; state = 'idle'; t_on = 0.0; peak = 0.0; t_off = None
    gate = 10 ** ((p['gate'] - p['off']) / 20)
    for i in range(m):
        v = pk[i]
        fast = v if v > fast else fast * kF + v * (1 - kF)
        slow = slow * (kA if fast > slow else kR) + fast * (1 - (kA if fast > slow else kR))
        e = 20 * np.log10(max(fast, 1e-6)) + p['off']
        es = 20 * np.log10(max(slow, 1e-6)) + p['off']
        t = i * dt
        onset = fast > gate and (state != 'note' and fast > gate) or (fast > gate and e - es > p['onDb'])
        if onset and (state != 'note' or t - t_on > 0.05):
            state = 'note'; t_on = t; peak = fast; t_off = None
        if state == 'note':
            peak = max(peak, fast)
            if t - t_on < p['settle']: gain = float(np.clip(target - max(es, e - 12.0), lo, hi))
            if fast < gate or 20 * np.log10(max(fast, 1e-9) / peak) < -p['offDb']:
                state = 'release'; t_off = t
        if state == 'release' and t - t_off > p['holdMs'] / 1000.0:
            gain += (idle - gain) * (1 - np.exp(-dt / p['tau']))
            if abs(gain - idle) < 0.01: state = 'idle'
        E[i] = e; G[i] = gain
    # meter display ballistics: instant rise, fall at most fallDb per second
    D = np.copy(E)
    for i in range(1, m):
        D[i] = max(E[i], D[i - 1] - p['fallDb'] * dt)
    D = np.clip(D, -42.0, 0.0)
    if frame_t is not None:
        idx = np.clip((np.asarray(frame_t) / dt).astype(int), 0, m - 1)
        return D[idx], G[idx]
    return D, G


def load(name, inp):
    d = np.load(os.path.join(HERE, '..', 'build', 'au_%s.npz' % name))
    x = np.fromfile(os.path.join(REF, inp), np.float32).astype(float)
    return d, x


def score(p, quiet=True):
    tot = []
    for name, inp in (('steady', 'steady.f32'), ('bass', 'bassline.f32')):
        d, x = load(name, inp)
        D, G = simulate(x, p, frame_t=d['t'])
        mtr = d['meterDb']; rid = d['rider']
        em = np.abs(D - mtr); er = np.abs(G - rid)
        if not quiet:
            print('%-7s meter |err| median %.2f dB, 90%% %.2f   rider |err| median %.2f dB, 90%% %.2f' % (
                name, np.median(em), np.percentile(em, 90), np.median(er), np.percentile(er, 90)))
        tot += [np.mean(np.minimum(em, 10)), np.mean(np.minimum(er, 10)) * 2]
    return float(np.sum(tot))


def main():
    p = dict(P0)
    if len(sys.argv) > 1 and sys.argv[1] == 'fit':
        keys = ['fc', 'relF', 'attS', 'relS', 'off', 'onDb', 'settle', 'offDb', 'holdMs', 'tau', 'fallDb']
        x0 = np.log([p[k] for k in keys])
        def f(z):
            q = dict(p); q.update({k: float(np.exp(v)) for k, v in zip(keys, z)})
            return score(q)
        r = optimize.minimize(f, x0, method='Nelder-Mead', options=dict(maxfev=600, xatol=1e-3, fatol=1e-3))
        p.update({k: float(np.exp(v)) for k, v in zip(keys, r.x)})
        print({k: round(v, 4) for k, v in p.items()})
    print('score %.3f' % score(p, quiet=False))


if __name__ == '__main__':
    main()
