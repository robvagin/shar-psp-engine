import re,sys
sc=None;last=-9;t0=None
for l in open(sys.argv[1],errors='replace'):
    m=re.match(r'SCENE (\S+)',l)
    if m: sc=m.group(1); continue
    if sc!='WJ_drop': continue
    if l.startswith('CAMFLIP'): print('   ',l.strip()); continue
    m=re.search(r'CTRLYAW t=(\S+) held=(\S+) view=(\S+) camYaw=(\S+) busy=(\d) bt=\S+ spd=(\S+) on=\d frame=(\S+) walk=(\S+) pos=(\S+),(\S+)',l)
    if m:
        t=float(m.group(1)); t0=t0 or t
        if t-last>=0.3 and t-t0>3: last=t; print(f't={t-t0:5.2f} view={float(m.group(3)):5.0f} cam={float(m.group(4)):5.0f} busy={m.group(5)} spd={m.group(6)} frame={float(m.group(7)):5.0f} walk={float(m.group(8)):5.0f} pos={m.group(9)},{m.group(10)}')
