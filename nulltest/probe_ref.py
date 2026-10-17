import sys, numpy as np
import pedalboard as pb
WS = r"C:\Program Files\Common Files\VST3\WaveShell1-VST3 17.1_x64.vst3"
try:
    names = pb.VST3Plugin.get_plugin_names_for_file(WS)
    print(len(names), [n for n in names if 'vox' in n.lower() or 'RVox' in n][:20])
except Exception as e:
    print('names failed', repr(e)); sys.exit(1)
for n in names:
    if n.lower().startswith('rvox') or 'renaissance vox' in n.lower():
        print('loading', n)
        p = pb.load_plugin(WS, plugin_name=n)
        print(p.name, list(p.parameters.keys()))
        for k, v in p.parameters.items(): print(k, v.range if hasattr(v,'range') else '', v.raw_value, v.string_value)
        x = (0.1*np.sin(2*np.pi*1000*np.arange(48000)/48000)).astype(np.float32)[None, :]
        y = p(x, 48000, reset=True)
        print(y.shape, float(np.abs(y).max()))
        break
