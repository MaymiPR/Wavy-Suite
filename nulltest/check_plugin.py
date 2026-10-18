"""The built WVox.vst3, hosted through pedalboard, against the offline DSP renderer (they must be identical)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, pedalboard as pb, nulltest_wvox as N, refhost as R
FS = 48000
VST = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "plugins", "WVox.vst3"))
p = pb.load_plugin(VST)
print('loaded', p.name, 'params', [k for k in p.parameters.keys()][:8], 'latency', p.reported_latency_samples)
S = N.signals(quick=True)
for name, x in S.items():
    for comp, gate, gain in ((-24, None, 0), (-12, -30, -3)):
        p.comp = float(comp); p.gate = -80.0 if gate is None else float(gate); p.gain = float(gain)
        y = p(x[None, :].astype(np.float32), FS, buffer_size=512, reset=True)[0]
        m = N.ours(x, FS, comp, gate, gain)[0]
        print('%-26s %4d/%s  plug-in vs offline %7.1f dB' % (name, comp, 'off' if gate is None else gate, N.residual_db(y, m, int(0.1 * FS))))
