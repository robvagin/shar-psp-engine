import math,sys
rows=[l.rstrip('\n').split('\t') for l in open(sys.argv[1])][1:]
lo=int(sys.argv[2]); hi=int(sys.argv[3])
d=[(int(r[0]),float(r[2]),float(r[4]),math.degrees(math.atan2(float(r[10]),float(r[12]))),float(r[8])) for r in rows]
w=lambda a:(a+180)%360-180
for i in range(20,len(d),10):
    f,x,z,c,cy=d[i]
    if not lo<=f<=hi: continue
    p=d[i-10]; dt=(f-p[0])/50; v=math.hypot(x-p[1],z-p[2])/dt
    if v<1.5: continue
    om=w(c-p[3])/dt; h=math.degrees(math.atan2(x-p[1],z-p[2])); rel=w(h-c); s=math.sin(math.radians(rel))
    print(f'  {f:4d} v={v:4.1f} ω={om:6.1f} rel={rel:6.1f} Ω_eff={abs(om/s) if abs(s)>.3 else float("nan"):6.1f} camY={cy:5.2f}')
