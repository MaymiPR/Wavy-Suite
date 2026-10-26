"""The reference Bass Rider's applied gain on musical material, for fitting WBass Rider's riding model alongside the
probes: the bass line (reference/wbassrider/bassline.f32) and synthetic bass parts (brcompare.synth_notes), at the
defaults and a few settings; cached as 10 ms medians where the input is loud enough to read a gain
(measurements/wbassrider/traces.npz).

    python -I br_traces.py            render the missing traces
    import br_traces as T; T.load()   [(name, settings, block centres in samples, reference gain per block), ...]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'traces.npz')
FS = 48000
BLOCK = 480


def material(name):
    import brcompare as C
    if name == 'bassline': return C.bassline()
    return C.synth_notes(seed=int(name.split('_')[1]), secs=10.0)


CASES = [('bassline', {}), ('synth_3', {}), ('synth_4', {}), ('synth_5', {}),
         ('bassline', {'response': 'Fast'}), ('synth_4', {'response': 'Fast'}),
         ('bassline', {'sensitivity': 90.0}), ('synth_5', {'sensitivity': 20.0}),
         ('bassline', {'artifacts': 15.0}), ('synth_3', {'artifacts': 85.0}),
         ('synth_9', {}), ('synth_10', {}), ('synth_11', {'response': 'Fast'}), ('synth_12', {'sensitivity': 70.0})]


def blocks(x, g):
    env = np.convolve(x.astype(float) ** 2, np.ones(96) / 96, 'same')
    idx, val = [], []
    for s in range(int(0.05 * FS), len(x) - BLOCK, BLOCK):
        ok = env[s:s + BLOCK] > 10 ** (-55 / 10)
        if ok.mean() > 0.8:
            idx.append(s + BLOCK // 2); val.append(np.median(g[s:s + BLOCK][ok]))
    return np.array(idx), np.array(val)


def render_all():
    import measure_bassrider as M
    have = {}
    if os.path.exists(OUT):
        d = np.load(OUT, allow_pickle=True); have = {k: (i, v) for k, i, v in zip(d['keys'], d['idx'], d['val'])}
    keys, I, V = [], [], []
    for name, st in CASES:
        key = json.dumps([name, st])
        if key not in have:
            x = material(name); s2 = dict(st)
            if 'response' in s2: s2['response'] += ' '
            have[key] = blocks(x, M.gtrace(x, M.render(x, **s2)))
            print('rendered', key, flush=True)
        keys.append(key); I.append(have[key][0]); V.append(have[key][1])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    np.savez(OUT, keys=np.array(keys, dtype=object), idx=np.array(I, dtype=object), val=np.array(V, dtype=object))


def load():
    d = np.load(OUT, allow_pickle=True)
    out = []
    for k, i, v in zip(d['keys'], d['idx'], d['val']):
        name, st = json.loads(k); out.append(dict(name=name, settings=st, idx=np.asarray(i, int), ref=np.asarray(v, float)))
    return out


if __name__ == '__main__':
    render_all()
