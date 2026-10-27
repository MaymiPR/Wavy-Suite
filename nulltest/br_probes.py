"""The note-detector probe set for fitting WBass Rider's riding model: short synthetic signals (steady notes, legato
dips, decays, re-attacks, gaps) at several Response / Sensitivity / Artifacts settings, each rendered once through the
reference and cached with the reference's applied gain (measurements/wbassrider/probes.npz: data from the reference's
audio only, no audio kept but the probe signals' recipes, rebuilt here).

    python -I br_probes.py            render the missing probes (fresh instances; about 10 minutes)
    import br_probes as P; P.load()   [(recipe, settings, read times, reference gains), ...]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'probes.npz')
FS = 48000


def build(recipe):
    """recipe: a list of parts, each (kind, a, b, secs): 'hold' at a dBFS, 'ramp' from a to b dB, 'gap' of silence;
    200 Hz unless the part says otherwise; the phase runs on across parts unless 'reset' follows a part"""
    out = []; ph = 0.0
    for part in recipe:
        kind, a, b, secs = part[:4]; f = part[4] if len(part) > 4 else 200.0
        n = int(round(secs * FS)); t = np.arange(n) / FS
        if kind == 'gap': out.append(np.zeros(n)); ph = 0.0; continue
        lev = a + (b - a) * t / max(secs, 1e-9) if kind == 'ramp' else np.full(n, float(a))
        out.append(10 ** (lev / 20) * np.sin(2 * np.pi * f * t + ph)); ph += 2 * np.pi * f * n / FS
    return np.concatenate(out).astype(np.float32)


def probes():
    P = []
    def add(recipe, read, **st): P.append(dict(recipe=recipe, read=read, settings=st))
    for resp in ('Slow', 'Fast'):
        for sens in (0.0, 25.0, 50.0, 75.0, 100.0):
            for X in (1, 2, 3, 4, 6, 8, 10, 12, 13, 14, 16):
                for L in (-6.5, -8.0, -12.0):
                    add([('hold', -6, 0, 0.6), ('hold', -6 - X, 0, 0.3), ('hold', L, 0, 0.6)], [0.5, 0.85, 1.3], response=resp, sensitivity=sens)
    for resp, sens, X in (('Slow', 50.0, 20), ('Fast', 50.0, 6), ('Slow', 100.0, 6), ('Fast', 0.0, 20)):
        for T in (2, 5, 10, 15, 20, 25, 30, 50):
            add([('hold', -6, 0, 0.6), ('hold', -6 - X, 0, T / 1000), ('hold', -6.5, 0, 0.8)], [0.5, 1.1 + T / 1000], response=resp, sensitivity=sens)
    for T in (0.25, 0.5, 1.0, 2.0, 4.0):
        for D in (-2, 0, 1, 2, 3, 4, 6, 8):
            add([('ramp', -6, -16, T), ('gap', 0, 0, 0.002), ('hold', -16 + D, 0, 0.6)], [T - 0.05, T + 0.4])
    for T in (0.01, 0.02, 0.04, 0.07, 0.1, 0.15, 0.2, 0.3, 0.5, 0.8, 1.2):
        for L in (-24, -20, -18, -16, -14, -12, -10, -8, -6):
            add([('hold', -6, 0, 0.6), ('hold', -26, 0, T), ('gap', 0, 0, 0.002), ('hold', L, 0, 0.5)], [0.6 + T + 0.3])
    for art in (0.0, 25.0, 50.0, 75.0, 100.0):
        for L in (-8, -12, -16, -20, -24, -28):
            add([('hold', -6, 0, 0.6), ('hold', -40, 0, 0.3), ('hold', L, 0, 0.6)], [1.3], artifacts=art)
    for D in (0.05, 0.5):
        for G in (0.03, 0.1, 0.3):
            for L in (-12, -16, -20, -28, -36, -46):
                add([('hold', -6, 0, D), ('gap', 0, 0, G), ('hold', L, 0, 0.6)], [D + G + 0.1, D + G + 0.5])
    for PK in (-6.0, -20.0):
        for G in (0.07, 0.1, 0.13, 0.2, 0.4):
            for d in (8, 14, 22, 30, 37, 40):
                add([("hold", PK, 0, 0.5, 100.0), ("gap", 0, 0, G), ("hold", PK - d, 0, 0.5, 100.0)], [0.5 + G + 0.3])
    # the pitch-dependent timing (the note level is a maximum over a window of 1024 / 2048 / 4096 samples picked by
    # the pitch): dips of T at several pitches, and legato drops under the note-off level read over time
    for f in (40.0, 60.0, 80.0, 100.0, 140.0, 200.0):
        for T in (0.02, 0.03, 0.045, 0.06, 0.09, 0.12):
            add([('hold', -6, 0, 0.6, f), ('hold', -26, 0, T, f), ('hold', -6.5, 0, 0.8, f)], [0.5, 0.6 + T + 0.5], target=-24.0)
        add([('hold', -6, 0, 0.6, f), ('hold', -48, 0, 0.6, f)], [0.5, 0.6, 0.62, 0.64, 0.66, 0.7, 0.75, 0.8, 0.9], target=-24.0)
    # legato pitch changes at Response Fast (re-rides just above the window boundaries, 40 .. 45 and 80 .. 90 Hz)
    for f1 in (60.0, 100.0, 150.0):
        for f2 in (35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 110, 120, 130, 150, 170, 200, 240, 290):
            add([('hold', -6, 0, 0.6, f1), ('hold', -12, 0, 0.8, float(f2))], [0.45, 1.1], response='Fast')
    for d in (3, 6, 14):
        for f2 in (85.0, 100.0, 60.0):
            add([('hold', -6, 0, 0.6, 100.0), ('hold', -6 - d, 0, 1.0, f2)], [0.45, 1.2])
    for F in (-20, -30, -40):
        for L in (-10, -14, -16, -18, -20):
            if L > F + 1: add([('hold', -6, 0, 0.6), ('hold', F, 0, 0.3), ('hold', L, 0, 0.6)], [1.3])
    # how long the gain follows a decaying note, by Sensitivity (the same at Slow and Fast, and at any pitch): notes
    # from silence decaying r dB/s; a slow fade is followed throughout, a faster decay until it is a margin under a
    # one-pole follower of the note level
    DREAD = [0.33, 0.36, 0.40, 0.45, 0.55, 0.70, 0.90, 1.10]
    for sens in (0.0, 25.0, 50.0, 75.0, 100.0):
        for r in (6, 9, 10, 12, 15, 20, 30, 60):
            add([('gap', 0, 0, 0.3), ('ramp', -14, -14 - r, 1.0, 100.0), ('gap', 0, 0, 0.2)], DREAD, sensitivity=sens)
    for sens in (25.0, 75.0, 100.0):
        for r in (10, 15, 30):
            add([('gap', 0, 0, 0.3), ('ramp', -14, -14 - r, 1.0, 100.0), ('gap', 0, 0, 0.2)], DREAD, sensitivity=sens, response='Fast')
    for f in (50.0, 200.0):
        for r in (12, 30):
            add([('gap', 0, 0, 0.3), ('ramp', -14, -14 - r, 1.0, f), ('gap', 0, 0, 0.2)], DREAD, sensitivity=75.0)
    # a new note on a decaying one (how real notes are re-attacked): a note from silence decaying r dB/s steps up X dB
    # at T (Target -20 keeps the law inside the range); read 100 .. 50 ms before the step (the look-ahead moves the gain up to 40 ms early) and 110 .. 160 ms after
    for resp in ('Slow', 'Fast'):
        for sens in (0.0, 50.0, 100.0):
            for r in (10, 30):
                for T in (0.1, 0.2, 0.4, 0.8):
                    for X in (1, 2, 3, 4.5, 6, 8):
                        add([('gap', 0, 0, 0.2), ('ramp', -16, -16 - r * T, T, 100.0), ('ramp', -16 - r * T + X, -16 - r * 2.0 + X, 2.0 - T, 100.0)],
                            [0.2 + T - 0.1, 0.2 + T + 0.11], response=resp, sensitivity=sens, target=-20.0)
    return P


def render_all():
    import measure_bassrider as M
    P = probes()
    have = {}
    if os.path.exists(OUT):
        d = np.load(OUT, allow_pickle=True)
        have = {k: v for k, v in zip(d['keys'], d['gains'])}
    keys, gains = [], []
    for i, p in enumerate(P):
        key = json.dumps([p['recipe'], p['read'], p['settings']])
        if key not in have:
            x = build(p['recipe'])
            st = dict(p['settings'])
            if 'response' in st: st['response'] = st['response'] + ' '
            g = M.gtrace(x, M.render(x, **st))
            w = 480 if len(p['read']) > 3 else 2400
            have[key] = np.array([np.median(g[int(t * FS):int(t * FS) + w]) for t in p['read']])
            if i % 20 == 0: print('%d / %d' % (i, len(P)), flush=True)
        keys.append(key); gains.append(have[key])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    np.savez(OUT, keys=np.array(keys, dtype=object), gains=np.array(gains, dtype=object))
    print('%d probes in %s' % (len(keys), OUT))


def load():
    d = np.load(OUT, allow_pickle=True)
    out = []
    for k, g in zip(d['keys'], d['gains']):
        recipe, read, st = json.loads(k)
        out.append(dict(recipe=recipe, read=read, settings=st, ref=np.asarray(g, float)))
    return out


if __name__ == '__main__':
    render_all()
