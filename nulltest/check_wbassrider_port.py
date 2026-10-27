"""The C++ ride (WBass Rider's processor, run by WBassRiderRender) against the fitting model (brmodel.c with the fit in
measurements/wbassrider/detector_fit.json): the same material, the gain applied by each, sample by sample. They are the
same model, so they should agree to rounding.

    python -I check_wbassrider_port.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import brmodel as B
import br_fit as F
import brcompare as C
import measure_bassrider as M
import nulltest_wbassrider as N
import br_probes as PR, bass_corpus as K

g, td = F.load_fit()
for name, x, st in (('bass line', C.bassline(), {}), ('synthetic part 6', C.synth_notes(seed=6, secs=10.0), {}),
                    ('synthetic part 7, Fast, Sensitivity 70', C.synth_notes(seed=7, secs=10.0), {'response': 'Fast', 'sensitivity': 70.0}),
                    ('bass line, Artifacts 20', C.bassline(), {'artifacts': 20.0}),
                    ('42 Hz decay, Fast (a band re-ride)', PR.build([('gap', 0, 0, 0.2), ('ramp', -8, -56, 1.6, 42.0)]), {'response': 'Fast'}),
                    ('corpus part 21, Fast, Sensitivity 25', K.part(21)[0], {'response': 'Fast', 'sensitivity': 25.0})):
    resp = st.get('response', 'Slow'); sens = float(st.get('sensitivity', 50.0))
    gm = B.ride(x, artifacts=float(st.get('artifacts', 50.0)), sensitivity=sens, response=resp, **dict(g, **F.setting_params(td, resp, sens)))
    y = N.render_ours(x[None], **{k: (v + ' ' if k == 'response' else v) for k, v in st.items()})[0]
    gp = M.gtrace(x, y)
    env = np.convolve(x.astype(float) ** 2, np.ones(96) / 96, 'same'); ok = env > 10 ** (-60 / 10); ok[:4800] = False
    d = np.abs(gp - gm)[ok]
    print('%-40s plug-in vs model gain: median %.4f dB, 99%% %.4f, max %.3f' % (name, np.median(d), np.percentile(d, 99), d.max()))
