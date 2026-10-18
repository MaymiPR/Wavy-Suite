import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import json, numpy as np, refhost as R
FS = 48000
st = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'results.json')))['static']
lv = np.array(st['levels']); G24 = np.array(st['gain_db']['-24'])
def level_from_gain(g_db):                 # invert the static law (comp -24): sine-peak level that gives this gain
    return np.interp(-g_db, -G24, lv)
def inst_gain(x, y):
    with np.errstate(divide='ignore', invalid='ignore'):
        g = 20*np.log10(np.abs(y/x))
    return np.where(np.abs(x) > 0.2*np.abs(x).max(), g, np.nan)
n = int(1.6 * FS); t = np.arange(n) / FS
amp = 10**(-20/20)
sigs = {'sine': amp*np.sin(2*np.pi*1000*t),
        'square': amp*np.sign(np.sin(2*np.pi*1000*t)),
        'pulse10': amp*(np.sin(2*np.pi*1000*t) > np.cos(np.pi*0.2)).astype(float)}
for k, x in sigs.items():
    y = R.render(x.astype(np.float32), FS, comp=-24)[0]
    g = inst_gain(x, y)
    L = level_from_gain(g)
    seg = slice(FS, FS+48)                 # one period at 1 kHz, steady state
    print(k, 'level (dB) over one period, samples with |x| > 0.2 peak:')
    print(np.round(L[seg], 2))
