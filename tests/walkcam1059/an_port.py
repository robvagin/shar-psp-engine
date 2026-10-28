import re,sys
last=-9;t0=None;step=float(sys.argv[2]) if len(sys.argv)>2 else .5
for l in open(sys.argv[1],errors='replace'):
    m=re.search(r'CTRLYAW t=(\S+) held=(\S+) view=(\S+) camYaw=(\S+) busy=(\d) bt=\S+ spd=(\S+) on=(\d) frame=(\S+) walk=(\S+) pos=(\S+),(\S+)',l)
    if not m: continue
    t=float(m.group(1)); t0=t0 or t
    if t-last<step: continue
    last=t; w=float(m.group(9)); v=float(m.group(3)); print(f't={t-t0:5.1f} view={v:6.0f} frame={float(m.group(8)):6.0f} busy={m.group(5)} spd={m.group(6)} walk={w:6.0f} walk-view={((w-v+180)%360-180):6.0f}')
