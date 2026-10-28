import re,sys
sc=None;R={}
for l in open(sys.argv[1],errors='replace'):
    m=re.match(r'SCENE (\S+)',l)
    if m: sc=m.group(1); continue
    m=re.search(r'CTRLYAW t=(\S+) held=(\S+) view=(\S+) camYaw=(\S+) busy=(\d) bt=\S+ spd=(\S+) on=(\d) frame=(\S+) walk=(\S+) pos=(\S+),(\S+)',l)
    if m and sc and sc.startswith('WD'): R.setdefault(sc,[]).append(m.groups())
for sc,rows in R.items():
    t0=float(rows[0][0]); print('==',sc); last=-9
    for r in rows:
        t=float(r[0])
        if t-last<0.7 or t-t0<1.2 or t-t0>5.8: continue
        last=t; w=float(r[8]); v=float(r[2]); print(f' t={t-t0:4.1f} view={v:5.0f} frame={float(r[7]):5.0f} busy={r[4]} spd={r[5]} walk={w:5.0f} walk-view={(w-v+180)%360-180:5.0f}')
