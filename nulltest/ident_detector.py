import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import json, numpy as np, refhost as R
FS = 48000
st = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'rvox', 'results.json')))['static']
lv = np.array(st['levels']); G24 = np.array(st['gain_db']['-24'])
def equiv_level(gain):                     # static-equivalent sine peak level for a measured steady gain at comp -24
    return float(np.interp(-gain, -G24, lv))   # G decreasing with level
def steady_gain(x, comp=-24):
    y = R.render(x.astype(np.float32), FS, comp=comp)[0]
    n = FS // 2
    return R.rms_db(y[-n:]) - R.rms_db(x[-n:])
n = int(1.6 * FS); t = np.arange(n) / FS
amp = 10 ** (-20 / 20)
rng = np.random.default_rng(3)
sigs = {
 'sine': amp * np.sin(2*np.pi*1000*t),
 'square': amp * np.sign(np.sin(2*np.pi*1000*t)),
 'triangle': amp * (2/np.pi) * np.arcsin(np.sin(2*np.pi*1000*t)),
 'noise_g': (lambda z: z * amp / z.std())(rng.standard_normal(n)),
 'pulse10pct': amp * (np.sin(2*np.pi*1000*t) > np.cos(np.pi*0.2)).astype(float),
 'two_sines': amp * 0.5 * (np.sin(2*np.pi*1000*t) + np.sin(2*np.pi*1700*t)),
}
for k, x in sigs.items():
    g = steady_gain(x)
    rms = R.rms_db(x[-FS//2:]); pk = 20*np.log10(np.abs(x).max())
    print(f'{k:12s} peak {pk:6.2f} rms {rms:6.2f} crest {pk-rms:5.2f}  gain {g:6.2f}  equiv sine-peak level {equiv_level(g):6.2f}  (rms+3.01={rms+3.01:6.2f})')
# short bursts at 12 kHz, -10 dB, comp -24: peak gain vs steady
print('steady gain at -10:', steady_gain(R.tone(FS, 12000, 1.0, -10)))
for ncyc in (1, 2, 4, 8, 16, 48, 96):
    x = np.concatenate([np.zeros(4800), R.tone(FS, 12000, ncyc/12000, -10), np.zeros(9600)]).astype(np.float32)
    y = R.render(x, FS, comp=-24)[0]
    a = 4800; b = a + int(ncyc/12000*FS)
    print(f'burst {ncyc:3d} cycles ({ncyc/12:.2f} ms): peak gain {20*np.log10(np.abs(y[a-8:b+8]).max()/np.abs(x[a:b]).max()):.2f} dB')
# amplitude modulation: gain modulation depth vs rate
for fm in (2, 5, 10, 20, 50, 100, 200, 500):
    am = 1 + 0.5 * np.sin(2*np.pi*fm*t)
    x = (amp * am * np.sin(2*np.pi*3000*t)).astype(np.float32)
    y = R.render(x, FS, comp=-24)[0]
    env_in = np.abs(x).reshape(-1, 48).max(axis=1); env_out = np.abs(y).reshape(-1, 48).max(axis=1)
    g = 20*np.log10(env_out[300:]/np.maximum(env_in[300:],1e-9)); g_in = 20*np.log10(env_in[300:])
    # modulation depth of the gain relative to what the static law predicts for the instantaneous level
    pred = np.interp(g_in, lv, G24)
    print(f'AM {fm:3d} Hz: gain p-p {g.max()-g.min():5.2f} dB, static-law p-p {pred.max()-pred.min():5.2f} dB')
