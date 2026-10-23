"""Fits H-Comp's In / Out needle (MeterCapture gl=1 films) with the law read off the step tests (MEASUREMENTS.md):
the level is the signal's peak over each frame's interval (dBFS, at least the last 10 ms block) on the strip's 71
frames, v = 70 (dB + 48) / 48. A rise (level above the needle) shows at once and starts a hold that decays from that
moment (amplitude x exp(-t / tau)). The first drop after a rise shows the level at once, unless the hold is still
higher; from then on every drop decays toward the level (amplitude, the same tau) until the next rise.
Private measurement helper.

    python -I fit_io_needle.py <signal.f32 | capture.npz:audio> <capture.npz> [...]   (pairs)
"""
import sys
import numpy as np
from scipy.optimize import minimize_scalar

FS = 48000
pairs = []
args = [a for a in sys.argv[1:] if not a.startswith('--')]
for sig, cap in zip(args[::2], args[1::2]):
    if sig.endswith(':audio'): x = np.abs(np.load(sig[:-6])['audio']).max(0)
    else: x = np.abs(np.fromfile(sig, np.float32))
    t, a, c = np.load(cap.replace('.npz', '_angles.npy'))
    k = np.round((a - 31.75) / (117.17 / 70))
    e = np.round(t * FS).astype(int); b = np.minimum(np.r_[0, e[:-1]], e - 480)
    pk = np.array([x[max(0, i):j].max() if j > max(0, i) else 0.0 for i, j in zip(b, e)])
    pairs.append((pk, t, k, cap))


def steps(amp): return 70.0 * (20 * np.log10(max(amp, 1e-9)) + 48.0) / 48.0


def sim(pk, t, tau):
    out = np.empty(len(pk)); shown = 0.0; P = tP = 0.0; E = 0.0; armed = False; last = t[0]
    for i, (L, tt) in enumerate(zip(pk, t)):
        Pn = P * np.exp(-(tt - tP) / tau)
        if L > shown:
            P, tP, E, armed, shown = L, tt, L, False, L
        elif not armed:
            if L < shown: armed = True
            shown = max(L, Pn); E = shown
        else:
            E = L + (E - L) * np.exp(-(tt - last) / tau)
            shown = max(L, E, Pn)
        last = tt
        out[i] = steps(shown)
    return out


def err(tau, q='round', detail=False):
    es = []
    for pk, t, k, cap in pairs:
        m = sim(pk, t, tau)
        p = np.clip(np.ceil(m - 1e-6) if q == 'ceil' else np.round(m), 0, 70)
        es.append(k - p)
    if detail: return es
    e = np.concatenate(es); return np.sqrt(np.mean(e ** 2))


for q in ('round', 'ceil'):
    r = minimize_scalar(lambda v: err(v, q), bounds=(0.1, 1.0), method='bounded', options=dict(xatol=1e-3))
    es = err(r.x, q, True); e = np.concatenate(es)
    print('quantise %-5s tau %.3f s (fall into silence %.1f dB/s): exact %.1f%%, within 1 %.1f%%, rms %.2f'
          % (q, r.x, 8.6859 / r.x, 100 * np.mean(e == 0), 100 * np.mean(abs(e) <= 1), np.sqrt(np.mean(e ** 2))))
    for (pk, t, k, cap), ee in zip(pairs, es):
        for lab, (a0, a1) in (('tones', (0, 16)), ('bursts', (16, 27.4)), ('drums', (27.4, 31.4)), ('voice', (32.4, 36.4)), ('all', (0, 99))):
            m = (t >= a0) & (t < a1)
            if m.sum(): print('   %-16s %-6s within 1 %3.0f%%  rms %.2f  mean %+.2f' % (cap.rsplit('/', 1)[-1], lab, 100 * np.mean(abs(ee[m]) <= 1), np.sqrt(np.mean(ee[m] ** 2)), ee[m].mean()))
