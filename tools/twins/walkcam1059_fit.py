import math,sys
def load(p):
    rows=[l.rstrip('\n').split('\t') for l in open(p)][1:]
    return [(int(r[0]),r[1],float(r[2]),float(r[4]),math.degrees(math.atan2(float(r[10]),float(r[12])))) for r in rows]
def wrap(a): return (a+180)%360-180
def course(d,i,k=4):
    f,s,x,z,c=d[i]; p=d[i-k]; dt=(f-p[0])/50; dx,dz=x-p[2],z-p[3]; v=math.hypot(dx,dz)/dt
    return (math.degrees(math.atan2(dx,dz)) if v>2.0 else None)
def fit(name,off,t0,tau,mode):
    d=load(f'build/twins/o4/{name}/wc.tsv'); st=0; err=[]; lp=None
    for i in range(len(d)):
        f,s,x,z,c=d[i]
        if lp is None: lp=c
        # фильтр 1-го порядка по углу, шаг 0.04 с; mode 'lp' — фильтр, 'delay' — запаздывание в кадрах
        if mode=='lp': lp=wrap(lp+wrap(c-lp)*(1-math.exp(-0.04/tau)))
        else:
            j=max(0,i-int(round(tau*25))); lp=d[j][4]
        if i>=4 and f>=t0:
            h=course(d,i)
            if h is not None: err.append(wrap(h-(lp+off)))
    return math.sqrt(sum(e*e for e in err)/len(err)) if err else None, len(err)
runs=[('wc1059_on126',0,126),('wc1059_on146',0,146),('wc1059_on156',0,156),('wc1059_on166',0,166),('wc1059_left200',-90,200)]
for mode in ('lp','delay'):
  for tau in (0.05,0.15,0.25,0.35,0.45,0.6,0.8):
    res=[fit(n,o,t,tau,mode) for n,o,t in runs]
    print(mode,tau,' '.join(f'{r[0]:6.1f}' if r[0] is not None else '   -  ' for r in res))
