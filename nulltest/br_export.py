"""Writes the note detector's fitted constants (measurements/wbassrider/detector_fit.json, from br_fit.py) into
plugins/WBassRider/WBassRiderRide.h, so the plug-in rides as the fitted model does.

    python -I br_export.py
"""
import os, re, json

HERE = os.path.dirname(os.path.abspath(__file__))
FIT = os.path.join(HERE, '..', 'measurements', 'wbassrider', 'detector_fit.json')
HDR = os.path.join(HERE, '..', 'plugins', 'WBassRider', 'WBassRiderRide.h')

d = json.load(open(FIT))
g = d['globals']
sens = ['0', '25', '50', '75', '100']
arr = lambda vals: '{ %s }' % ', '.join('%.4f' % v for v in vals)
block = """    // the note detector's constants (nulltest/br_fit.py, measurements/wbassrider/detector_fit.json; br_export.py)
    double slowAtt = %.4f, slowRel = %.4f, refRel = %.4f, hyst = %.4f, relMs = %.4f, holdMs = %.4f, onRise = %.4f,
           pitchPeriods = %.4f, offMs = %.4f, offRelMs = %.4f, winMs = %.4f, floorDb = -48.75;
    // at Sensitivity 0 .. 100 by 25: the rise over S that is a new note, and the margin over S that keeps the attack
    double onThr[5] %s, margin[5] %s;
    double depthSlow[5] %s, depthFast[5] %s;

""" % (g['slow_att'], g['slow_rel'], g['ref_rel'], g['hyst'], g['rel_ms'], g['settle'], g['on_rise'], 3.3, g['off_ms'], g.get('off_rel_ms', 60.0), g['win_ms'],
       arr([d['thr_u'][k] for k in sens]), arr([d['margin'][k] for k in sens]),
       arr([d['rel_depth']['Slow ' + k] for k in sens]), arr([d['rel_depth']['Fast ' + k] for k in sens]))
s = open(HDR, encoding='utf-8').read()
a = s.index("    // the note detector's constants"); b = s.index("private:")
s = s[:a] + block + s[b:]
open(HDR, 'w', encoding='utf-8').write(s)
print(block)
