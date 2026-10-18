"""WBass: joint fit of the gain-control model (wbass_model.generated7) on step trajectories, steady-state harmonics
and Decay-0 step renders at Freq 80. Usage: python fit_g7.py generated7 <place> <keys json> <x0 json> <lo json> <hi json>"""
import sys, json; sys.path.insert(0, r"C:/Users/wint1/Downloads/REA/Wavy/nulltest")
import numpy as np, wbass_model as M
from scipy.signal import sosfilt, hilbert
from scipy.optimize import least_squares
FS=48000; F=80.0
fname=sys.argv[1]; place=sys.argv[2]; fn=getattr(M,fname)
dyn=json.load(open(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/dyn_env70.json"))
raw=json.load(open(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/raw_decay0.json"))
lev=json.load(open(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/levels_decay0.json"))
st=np.load(r"C:/Users/wint1/Downloads/REA/Wavy/measurements/rbass/steps_80.npz")
harm=[(L,f0,np.array([complex(*v) for v in c])) for L,f0,c in raw if f0 in (10,15,22,33,48,64,80)]
harm+=[(L,float(f0s),np.array([complex(*v) for v in c])) for f0s,rows in lev.items() for L,c in rows if L in (-60,-36,-12,0)]
steps=[(st["x_%s_0" % f].astype(np.float64),st["gen_%s_0" % f]) for f in ("30","55","90")]
def comps(y,f0,nh=5):
    per=FS/f0; n=int(round(int(0.25*f0)*per)); seg=y[-n:]; t=(np.arange(n)+len(y)-n)/FS; w=np.hanning(n); w/=w.sum()/2
    return np.array([np.sum(seg*w*np.exp(-1j*(2*np.pi*f0*h*t-np.pi/2))) for h in range(1,nh+1)])
def dyntraj(P,key):
    L1,L2=[None if v=="None" else float(v) for v in key.split("_")[:2]]
    parts=[(0.3,L1),(0.6,L2),(0.6,L1)]
    env=np.concatenate([np.full(int(s*FS),0.0 if L is None else 10**(L/20)) for s,L in parts]); t=np.arange(len(env))/FS
    x=env*np.sin(2*np.pi*70*t)
    g=fn(x,F,P); r=sosfilt(M.sos(3,'high',F),sosfilt(M.sos(3,'high',F),sosfilt(M.sos(3,'low',F/3),x)))
    return (np.abs(hilbert(g))/np.maximum(np.abs(hilbert(r)),1e-12))[::48]
KEYS=json.loads(sys.argv[3]); X0=json.loads(sys.argv[4]); LO=json.loads(sys.argv[5]); HI=json.loads(sys.argv[6])
FIXED=dict(place=place,k2=0.0)
def mk(v): P=dict(FIXED); P.update({k:float(x) for k,x in zip(KEYS,v)}); return P
def err(v,show=False):
    P=mk(v); e=[]
    for key,rr in dyn.items():
        rr=np.array(rr); m=dyntraj(P,key)
        for base in (300,900):
            if key.startswith("None") and base==900: continue
            idx=base+np.r_[1:150:2,200,400]
            e+=list(0.5*20*np.log10(m[idx]/rr[idx]))      # dB errors, weight 0.5
    for L,f0,c in harm:
        x=10**(L/20)*np.sin(2*np.pi*f0*np.arange(int(0.5*FS))/FS); m=comps(fn(x,F,P),f0)
        for h,w in ((0,3.0),(2,1.0),(4,0.5)):
            q=np.log((m[h]+1e-30)/c[h]); e+=[w*8.686*q.real/3,w*np.degrees(np.angle(np.exp(1j*q.imag)))/20]
    for x,g in steps:
        m=fn(x,F,P); sl=slice(FS//20,None)
        e+=list(3*(g[sl]-m[sl])[::200]/np.sqrt(np.mean(g[sl]**2)))
    return np.array(e)
r=least_squares(err,X0,bounds=(LO,HI),x_scale=np.abs(np.array(X0))+1e-3,diff_step=1e-3,max_nfev=150)
P=mk(r.x); print(fname,place,"cost %.1f" % r.cost,{k:round(x,6) for k,x in zip(KEYS,r.x)})
json.dump(P,open(r"C:/Users/wint1/Downloads/REA/Wavy/build/fit_%s_%s.json" % (fname,place),"w"))
for x,g in steps:
    m=fn(x,F,P); sl=slice(FS//20,None); print("  step residual %.1f dB" % (10*np.log10(np.sum((g[sl]-m[sl])**2)/np.sum(g[sl]**2))))
for L,f0,c in harm[:14]:
    x=10**(L/20)*np.sin(2*np.pi*f0*np.arange(int(0.5*FS))/FS); m=comps(fn(x,F,P),f0)
    print("  L %g f0 %g  h1 %+.2f dB %+.1f deg  h3 %+.1f dB %+.0f deg" % (L,f0,8.686*np.log(abs(m[0]/c[0])),np.degrees(np.angle(m[0]/c[0])),8.686*np.log(abs(m[2]/c[2])),np.degrees(np.angle(m[2]/c[2]))))
