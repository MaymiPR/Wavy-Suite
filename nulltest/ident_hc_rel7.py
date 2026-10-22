"""WComp Phase 4: H-Comp's release, seventh form (hcfast7.c): as the fourth form (ident_hc_rel4.py: S the plain ~150 ms
average of R), but the release-time factor is a table of the charge fraction S / R. The release is at the Release
time once the average has caught up (steady tones, long compression, and the start-up, where S starts high), and much
shorter while the reduction is new (pitch spikes, drum hits).

    python -I ident_hc_rel7.py try          the hand-set start on the probes and the null-test material
    python -I ident_hc_rel7.py fit [nfev]   measurements/hcomp/release_law7.json
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import optimize
import ident_hc_rel4 as I4

I4.FORM = 7
I4.AVG = 1
I4.WX = np.array([0, 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0], float)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'measurements', 'hcomp', 'release_law7.json')
I4.OUT = OUT


def initial():
    base = np.array(json.load(open(os.path.join(os.path.dirname(I4.OUT), '..', '..', 'build', 'release_law4.avg.json')))['q'])
    nF, nA = len(I4.FX), len(I4.AX)
    W = np.interp(I4.WX, [0, 0.1, 0.2, 0.3, 0.45, 0.7, 0.9, 1.0], np.log([0.003, 0.01, 0.05, 0.15, 0.4, 0.7, 0.95, 1.0]))
    return np.concatenate([base[:nF + nA], W, [np.log(150.0), 14.0]])


def fit(nfev):
    cs = I4.load_cases()
    q0 = np.array(json.load(open(OUT))['q']) if os.path.exists(OUT) else initial()
    r = I4.residuals(q0, cs); print('start rms %.3f dB worst %.2f (%d cases)' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max(), len(cs)), flush=True)
    s = optimize.least_squares(I4.residuals, q0, args=(cs,), diff_step=1e-3, max_nfev=nfev)
    r = I4.residuals(s.x, cs); print('fit rms %.3f dB worst %.2f' % (np.sqrt(np.mean(r ** 2)), np.abs(r).max()), flush=True)
    json.dump({'q': list(map(float, s.x)), 'FX': I4.FX.tolist(), 'AX': I4.AX.tolist(), 'WX': I4.WX.tolist(), 'avg': 1, 'form': 7}, open(OUT, 'w'))
    I4.report(s.x)


if __name__ == '__main__':
    if sys.argv[1] == 'fit': fit(int(sys.argv[2]) if len(sys.argv) > 2 else 30)
    else:
        q = initial()
        cs = I4.load_cases()
        r = I4.residuals(q, cs); print('hand-set start: rms %.3f dB' % np.sqrt(np.mean(r ** 2)))
        json.dump({'q': list(map(float, q))}, open(OUT + '.try', 'w'))
