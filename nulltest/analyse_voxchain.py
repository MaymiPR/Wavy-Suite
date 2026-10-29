"""Reads campaign_voxchain.py's results (reference/wvoxchain/meas/campaign/*.json) and prints each section's tables:
the laws of every control over all its amounts, the level dependence, the pairs' interactions (how far a combination
departs from the stages acting independently), tempo, stereo input, sample rates, timing (MEASUREMENTS.md, WVox Chain
Phase 3, the full campaign).

    python -I analyse_voxchain.py [section ...]
"""
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from campaign_voxchain import FREQS, LEVELS, NOISE_LEVELS, FADER_STEPS, GRID, CLEAR
IN = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas', 'campaign')
BAND = (FREQS >= 30) & (FREQS <= 16000)


def load(sec):
    p = os.path.join(IN, sec + '.json')
    return json.load(open(p)) if os.path.exists(p) else []


def fi(f): return int(np.argmin(abs(FREQS - f)))


def t63(env, start, stop):
    seg = np.array(env[start:stop]); d = seg - seg[-1]
    if abs(d[0]) < 0.3: return None
    return int(np.argmax(np.abs(d) < 0.37 * abs(d[0])))


def single():
    R = load('single')
    if not R: return
    clear = np.array(next(r for r in R if r['kind'] == 'tf' and r['stage'] == 'clear')['magL'])
    print('\n== EQ: the gain over every colour clear (dB) at each fader step; the law of its largest deviation')
    for st, probes in (('bass', (30, 50, 100, 200, 1000)), ('treble', (500, 2000, 5000, 10000, 16000))):
        for c in (1, 2, 3):
            rows = sorted((r for r in R if r['kind'] == 'tf' and r['stage'] == st and r['colour'] == c), key=lambda r: r['fader'])
            D = np.array([np.array(r['magL']) - clear for r in rows])
            k = np.argmax(np.abs(D[-1] - D[10]) * BAND)                # the frequency the fader moves most
            print('  %s colour %d (largest at %.0f Hz):' % (st, c, FREQS[k]))
            print('    fader ' + ' '.join('%6d' % r['fader'] for r in rows))
            for q in probes + (FREQS[k],):
                print('    %5.0f ' % q + ' '.join('%+6.2f' % d[fi(q)] for d in D))
            g = D[:, k]
            for side, sel in (('cut', slice(0, 11)), ('boost', slice(10, 21))):
                v = np.array(FADER_STEPS[sel]); y = g[sel] - g[10]
                slope = np.sum(v * y) / max(np.sum(v * v), 1e-9)
                print('    %-5s: %.3f dB per step through 0, worst departure from that line %.2f dB' % (side, slope, np.max(np.abs(y - slope * v))))
    print('\n== Compress: the gain (dB) per input level (1 kHz tone, dBFS peak) over the gain with it clear')
    base = {r['freq']: np.array(r['gL']) for r in R if r['kind'] == 'steps' and r['stage'] == 'compress' and r['colour'] == 0}
    print('    level ' + ' '.join('%6d' % l for l in LEVELS))
    for c in (1, 2, 3):
        rows = sorted((r for r in R if r['kind'] == 'steps' and r['stage'] == 'compress' and r['colour'] == c and r['freq'] == 1000), key=lambda r: r['fader'])
        for r in rows:
            print('  c%d %+3d ' % (c, r['fader']) + ' '.join('%+6.2f' % v for v in np.array(r['gL']) - base[1000]))
        mk = [np.array(r['gL'])[0] - base[1000][0] for r in rows]
        print('  c%d makeup by fader (low level): %s' % (c, ' '.join('%+.2f' % m for m in mk)))
        outmax = [np.max(np.array(r['gL']) + np.array(LEVELS)) for r in rows]
        print('  c%d highest output level (dBFS peak) by fader: %s' % (c, ' '.join('%+.2f' % m for m in outmax)))
        for f0 in (100, 6000):
            for r in sorted((r for r in R if r['kind'] == 'steps' and r['stage'] == 'compress' and r['colour'] == c and r['freq'] == f0), key=lambda r: r['fader']):
                d = np.array(r['gL']) - base[f0] - (np.array(next(q for q in rows if q['fader'] == r['fader'])['gL']) - base[1000])
                print('    at %5d Hz, fader %+3d: differs from 1 kHz by %s' % (f0, r['fader'], ' '.join('%+.1f' % v for v in d)))
    print('\n== The wet stages: energy (dB, L / R) by fader; the step; the shape held (T60, first arrival)')
    for st in ('reverb', 'delay', 'pitch'):
        for c in (1, 2, 3):
            rows = sorted((r for r in R if r['kind'] == 'wet' and r['stage'] == st and r['colour'] == c), key=lambda r: r['fader'])
            eL = np.array([r['eL'] for r in rows]); v = np.array([r['fader'] for r in rows])
            p = np.polyfit(v, eL, 1)
            print('  %-6s c%d: %.4f dB per step, line residual max %.3f dB; at 0: L %+.2f R %+.2f; T60 %s; first %s ms; corr %s' % (
                st, c, p[0], np.max(np.abs(np.polyval(p, v) - eL)), rows[10]['eL'], rows[10]['eR'],
                sorted(set(r['t60'] for r in rows)), sorted(set(r['first_ms'] for r in rows)), sorted(set(round(r['corr'], 2) for r in rows))))
    print('\n== Sensitivity / Output: the gain per input level (noise, dB rms) at each fader step (dry default: Sub, Bite, Push)')
    for name in ('sensitivity', 'output'):
        rows = sorted((r for r in R if r['kind'] == 'steps' and r['stage'] == name), key=lambda r: r['fader'])
        ref = np.array(next(r for r in rows if r['fader'] == 0)['gL'])
        print('  %s    level ' % name + ' '.join('%6d' % l for l in NOISE_LEVELS))
        for r in rows:
            print('    %+3d ' % r['fader'] + ' '.join('%+6.2f' % v for v in np.array(r['gL']) - ref))
    print('\n== Balance: output rms L / R (dB) relative to centre')
    rows = sorted((r for r in R if r['stage'] == 'balance'), key=lambda r: r['fader'])
    c0 = next(r for r in rows if r['fader'] == 0)['rms']
    print('  ' + '  '.join('%+.1f: %+.2f/%+.2f' % (r['fader'], r['rms'][0] - c0[0], r['rms'][1] - c0[1]) for r in rows))


def levels():
    R = load('levels')
    if not R: return
    print('\n== Compress x Sensitivity: the gain (dB) per input level (noise, dB rms)')
    print('    level   ' + ' '.join('%6d' % l for l in NOISE_LEVELS))
    for c in (0, 1, 2, 3):
        for r in sorted((r for r in R if r.get('what') == 'compress x sensitivity' and r['colour'] == c), key=lambda r: r['sens']):
            print('  c%d s%+3d ' % (c, r['sens']) + ' '.join('%+6.2f' % v for v in r['gL']))
    print('\n== The EQ at level: the gain per tone level (dBFS peak), Compress clear (c0) or Spank (c2), less the gain at -45')
    print('    level            ' + ' '.join('%6d' % l for l in LEVELS))
    for r in sorted((r for r in R if r.get('what') == 'eq at level'), key=lambda r: (r['stage'], r['colour'], r['fader'], r['freq'], r['comp'])):
        g = np.array(r['gL'])
        print('  %-6s c%d %+3d %5d c%d ' % (r['stage'], r['colour'], r['fader'], r['freq'], r['comp']) + ' '.join('%+6.2f' % v for v in g - g[0]) + '   (at -45: %+.2f)' % g[0])
    print('\n== The de-esser x Sensitivity: gain per tone level (dBFS peak), less the gain at -45')
    for r in sorted((r for r in R if r.get('what') == 'deesser x sensitivity'), key=lambda r: (r['freq'], r['sens'])):
        g = np.array(r['gL'])
        print('  %5d Hz s%+3d ' % (r['freq'], r['sens']) + ' '.join('%+6.2f' % v for v in g - g[0]))
    print('\n== The wet stages at level: energy per unit input (dB) by impulse amplitude')
    for st in ('reverb', 'delay', 'pitch'):
        for c in (1, 2, 3):
            rows = sorted((r for r in R if r.get('what') == 'wet at level' and r['stage'] == st and r['colour'] == c), key=lambda r: r['amp'])
            print('  %-6s c%d: %s' % (st, c, '  '.join('%.2f: L %+.2f R %+.2f' % (r['amp'], r['eL'], r['eR']) for r in rows)))


def pairs():
    R = load('pairs')
    S = load('single')
    if not R: return
    clear = np.array(next(r for r in S if r['kind'] == 'tf' and r['stage'] == 'clear')['magL'])
    one = {(r['stage'], r['colour'], r['fader']): np.array(r['magL']) - clear for r in S if r['kind'] == 'tf' and r['stage'] in ('bass', 'treble')}
    print('\n== Bass x Treble: the combination against the sum of each alone (dB, 30 Hz .. 16 kHz)')
    worst = []
    for r in R:
        if r.get('pair') != 'bass x treble': continue
        pred = one[('bass', r['cb'], r['vb'])] + one[('treble', r['ct'], r['vt'])]
        d = np.max(np.abs((np.array(r['magL']) - clear) - pred)[BAND])
        worst.append((d, r['cb'], r['ct'], r['vb'], r['vt']))
    worst.sort(reverse=True)
    print('  largest departures: ' + '; '.join('%.2f dB (Bass c%d %+d, Treble c%d %+d)' % (d, cb, vb, ct, vt) for d, cb, ct, vb, vt in worst[:8]))
    print('  median %.3f dB over %d combinations' % (np.median([w[0] for w in worst]), len(worst)))
    print('\n== EQ x Compress: the gain per tone level with both, less (EQ alone at that frequency + Compress alone)')
    Cs = {(r['colour'], r['fader'], r['freq']): np.array(r['gL']) for r in S if r['kind'] == 'steps' and r['stage'] == 'compress'}
    print('    level                 ' + ' '.join('%6d' % l for l in LEVELS))
    for r in sorted((r for r in R if r.get('pair', '').endswith('x compress')), key=lambda r: (r['pair'], r['c'], r['cc'], r['v'], r['vc'])):
        st = r['pair'].split()[0]
        eq = one[(st, r['c'], r['v'])][fi(r['freq'])]
        comp = Cs.get((r['cc'], r['vc'], r['freq']))
        c0 = Cs.get((0, 0, r['freq']))
        if comp is None or c0 is None: continue
        d = np.array(r['gL']) - (comp + eq)
        print('  %-17s c%d %+3d / c%d %+3d ' % (r['pair'], r['c'], r['v'], r['cc'], r['vc']) + ' '.join('%+6.2f' % v for v in d))
    print('\n== The wet stages under other settings: their energy (dB, L / R) and its change over the stage alone (colour 2 / as listed)')
    alone = {(r['stage'], r['colour']): r for r in S if r['kind'] == 'wet' and r['fader'] == 0}
    for r in sorted((r for r in R if r['kind'] == 'wet' and r.get('pair') != 'wet combination'), key=lambda r: r['pair']):
        st = r['pair'].split(' x ')[1]
        a = alone[(st, r['colour'])]
        tag = ' '.join('%s=%s' % (k, r[k]) for k in ('cc', 'vc', 'ce', 'v', 'balance', 'sens', 'out') if k in r)
        print('  %-20s c%d %-14s L %+6.2f (%+5.2f)  R %+6.2f (%+5.2f)  T60 %s (alone %s)' % (r['pair'], r['colour'], tag, r['eL'], r['eL'] - a['eL'], r['eR'], r['eR'] - a['eR'], r['t60'], a['t60']))
    print('\n== The wet stages together: the combination against the sum of each alone (first 0.5 s, residual energy relative to the combination)')
    W = {(r['cr'], r['cd'], r['cp']): r for r in R if r.get('pair') == 'wet combination'}
    for key, r in sorted(W.items()):
        parts = [W.get((key[0], 0, 0)), W.get((0, key[1], 0)), W.get((0, 0, key[2]))]
        parts = [p for p, c in zip(parts, key) if c > 0]
        if len(parts) < 2: continue
        s = sum(np.array(p['ir']) for p in parts); w = np.array(r['ir'])
        res = 10 * np.log10(np.sum((w - s) ** 2) / max(np.sum(w ** 2), 1e-30) + 1e-30)
        print('  reverb c%d delay c%d pitch c%d: residual %+.1f dB' % (key[0], key[1], key[2], res))


def stereo():
    R = load('stereo')
    if not R: return
    print('\n== Stereo input: each output from each input (dB at 100 Hz / 1 kHz / 5 kHz), and the output rms (L / R)')
    for r in sorted(R, key=lambda r: (r['config'], r['how'])):
        def at(m): return '/'.join('%+.1f' % m[fi(q)] for q in (100, 1000, 5000)) if m else '   -   '
        print('  %-12s %-9s L<-L %s  R<-L %s  R<-R %s  L<-R %s  rms %+.1f/%+.1f' % (r['config'], r['how'], at(r['LfromL']), at(r['RfromL']), at(r['RfromR']), at(r['LfromR']), r['rms'][0], r['rms'][1]))


def rates():
    R = load('rates')
    if not R: return
    print('\n== Sample rates: the response (dB) against 48 kHz, worst over 30 Hz .. 16 kHz')
    ref = {r['config']: np.array(r['magL']) for r in R if r['fs'] == 48000}
    for r in sorted(R, key=lambda r: (r['config'], r['fs'])):
        if r['fs'] == 48000: continue
        d = np.array(r['magL']) - ref[r['config']]
        k = np.argmax(np.abs(d) * BAND)
        print('  %-13s %6d: worst %+.2f dB at %.0f Hz; at 10k %+.2f, 16k %+.2f' % (r['config'], r['fs'], d[k], FREQS[k], d[fi(10000)], d[fi(16000)]))


def timing():
    R = load('timing')
    if not R: return
    print('\n== Timing: the gain change at the rise (attack) and the fall (release): t63 in ms, the gain reduction held')
    for r in sorted(R, key=lambda r: (r['what'], r.get('colour', 0), r.get('fader', 0), r.get('freq', 0), r['lo'], r['hi'], r['on'])):
        e = r['env']; a = 500; b = 500 + int(r['on'] * 1000)
        ta = t63(e, a, b); tr = t63(e, b, min(len(e), b + 1500))
        tag = 'c%d %+3d' % (r['colour'], r['fader']) if r['what'] == 'compress' else '%5d Hz' % r['freq']
        print('  %-8s %s  %+d -> %+d dB, %4.0f ms: before %+.2f, held %+.2f, attack t63 %s ms, release t63 %s ms (after: %+.2f)' % (
            r['what'], tag, r['lo'], r['hi'], r['on'] * 1000, e[a - 5], e[b - 2], ta, tr, e[min(len(e) - 1, b + 1400)]))


def combos():
    R = load('combos')
    if not R: return
    print('\n== Random combinations and the factory presets on vocals: output rms L / R, peak, L / R correlation')
    for r in sorted(R, key=lambda r: r['combo']):
        print('  %-11s %s  rms %+6.2f/%+6.2f (in %+.1f)  peak %+.2f  corr %+.3f' % (r['combo'], ' '.join('%s=%s' % (k, v) for k, v in sorted(r['kw'].items())),
              r['rms'][0], r['rms'][1], r['rms_in'], r['peak'], r['corr']))


if __name__ == '__main__':
    secs = sys.argv[1:] or ['single', 'levels', 'pairs', 'stereo', 'rates', 'timing', 'combos']
    for s in secs: globals()[s]()
