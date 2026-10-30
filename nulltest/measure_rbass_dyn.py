"""WBass: gain trajectory of the reference through level steps of a 70 Hz tone (Decay 0), as the ratio of Hilbert
envelopes of the generated part and of the filtered input; writes measurements/rbass/dyn_env70.json."""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))     # (the repository)
import sys, json; sys.path.insert(0, ROOT + r"/nulltest")
import numpy as np, refhost as R, wbass_model as M
from scipy.signal import sosfilt, hilbert
FS=48000; F=80.0
out={}
for f0 in (70.0,):
    for (L1,L2) in ((None,-30),(None,-10),(None,0),(-40,-10),(-10,-40),(-10,-30),(0,-40)):
        parts=[(0.3,L1),(0.6,L2),(0.6,L1 if L1 is not None else None)]
        x=np.concatenate([np.zeros(int(s*FS),np.float32) if L is None else R.tone(FS,f0,s,L) for s,L in parts])
        # keep phase continuous: build as one sine with an amplitude envelope instead
        t=np.arange(len(x))/FS; env=np.concatenate([np.full(int(s*FS),0.0 if L is None else 10**(L/20)) for s,L in parts])
        x=(env*np.sin(2*np.pi*f0*t)).astype(np.float32)
        y0=R.render_rbass(x,FS,freq=F,intensity=0,hp=1,extra={8:0})[0].astype(np.float64); y1=R.render_rbass(x,FS,freq=F,intensity=-24,hp=1,extra={8:0})[0].astype(np.float64)
        g=(y0-y1)/(1-10**(-24/20))
        ref=sosfilt(M.sos(3,'high',F),sosfilt(M.sos(3,'high',F),sosfilt(M.sos(3,'low',F/3),x.astype(np.float64))))
        cs=np.abs(hilbert(g))/np.maximum(np.abs(hilbert(ref)),1e-12)
        key="%s_%s_%g" % (L1,L2,f0); out[key]=cs[::48].tolist()   # 1 ms resolution
        i1=int(0.3*FS); i2=int(0.9*FS)
        def at(ms,base): return 20*np.log10(cs[base+int(ms*48)])
        print("%-14s gain dB after step: " % key+" ".join("%dms %.1f" % (ms,at(ms,i1)) for ms in (2,5,10,20,40,80,200,500)),"| after 2nd step: "+" ".join("%dms %.1f" % (ms,at(ms,i2)) for ms in (2,5,10,20,40,80,200,500)) if L1 is not None else "",flush=True)
json.dump(out,open(ROOT + r"/measurements/rbass/dyn_env70.json","w"))
