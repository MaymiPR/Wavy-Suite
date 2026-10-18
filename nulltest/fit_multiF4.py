"""WBass: joint time-domain fit of wbass_model.generated7 over the fitting sets at Freq 40/80/160/256
(wbass_fitdata.py); writes build/multiF.json, the constants used in plugins/WBass/WBassDsp.h."""
import sys, json; sys.path.insert(0, r"C:/Users/wint1/Downloads/REA/Wavy/nulltest")
import numpy as np, wbass_model as M
from scipy.optimize import least_squares
FS=48000
sets=[]
tr=np.load(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/transients.npz")
for F in (80,160,256):
    for nm in ("kick","notes","riff"):
        for rep in range(3): sets.append((F,tr["x_%s_%d" % (nm,F)].astype(np.float64),tr["lin_%s_%d" % (nm,F)],"TR_%s" % nm))
for F in (40,80,160,256):
    d=np.load(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/fitdata_%d.npz" % F)
    for n in d.files:
        if n.startswith("gen_"): sets.append((F,d["x_"+n[4:]].astype(np.float64),d[n],n[4:]))
sl=slice(FS//10,None)
base=json.load(open(r"C:/Users/wint1/Downloads/REA/Wavy/build/twodet.json")); base.update(k2=json.load(open(r"C:/Users/wint1/Downloads/REA/Wavy/build/multiF2.json"))["k2"], sL=0.0)
keys=["C0","E0","p","cA","QA","ta1","tr1","ta2","tr2","alpha","k2","s1","s2","El0","pL","tLa","tLr","sL"]
lin={"s1","s2","alpha","sL"}
def mk(v):
    P=dict(base)
    for k,x in zip(keys,v): P[k]=float(x) if k in lin else (-float(np.exp(x)) if k=="k2" else float(np.exp(x)))
    return P
def resid(v):
    P=mk(v); o=[]
    for F,x,g,_ in sets:
        m=M.generated7(x,F,P); o.append((g[sl]-m[sl])[::6]/np.sqrt(np.mean(g[sl]**2)))
    return np.concatenate(o)
v0=np.array([base[k] if k in lin else np.log(abs(base[k])) for k in keys])
lo=np.array([-0.5 if k in ("s1","s2","sL") else 0.0 if k=="alpha" else -15 for k in keys]); hi=np.array([1.5 if k in ("s1","s2","sL") else 1.0 if k=="alpha" else 8 for k in keys])
r=least_squares(resid,np.clip(v0,lo+1e-6,hi-1e-6),bounds=(lo,hi),diff_step=1e-3,max_nfev=60)
P=mk(r.x); json.dump(P,open(r"C:/Users/wint1/Downloads/REA/Wavy/build/multiF4.json","w"))
print({k:round(P[k],6) for k in keys})
for F in (40,80,160,256):
    v=[10*np.log10(np.sum((g[sl]-M.generated7(x,F,P)[sl])**2)/np.sum(g[sl]**2)) for FF,x,g,n in sets if FF==F]
    print("F %d median %.1f worst %.1f" % (F,np.median(v),max(v)))
for F,x,g,n in sets:
    if n.startswith("TR_"):
        print("  F %d %s %.1f dB" % (F,n,10*np.log10(np.sum((g[sl]-M.generated7(x,F,P)[sl])**2)/np.sum(g[sl]**2))))
