import sys, math
rows=[l.rstrip('\n').split('\t') for l in open(sys.argv[1])][1:]
step=int(sys.argv[2]) if len(sys.argv)>2 else 4
lo=int(sys.argv[3]) if len(sys.argv)>3 else 0; hi=int(sys.argv[4]) if len(sys.argv)>4 else 10**9
d=[(int(r[0]),r[1],float(r[2]),float(r[4]),math.atan2(float(r[10]),float(r[12]))) for r in rows]
print('кадр стик  x      z    скор м/с  курс_хода°  камера°  ход-кам°')
for i in range(step,len(d),step):
    f,s,x,z,cy=d[i]
    if not lo<=f<=hi: continue
    p=d[i-step]; dt=(f-p[0])/50
    dx,dz=x-p[2],z-p[3]; v=math.hypot(dx,dz)/dt
    h=math.degrees(math.atan2(dx,dz)) if v>0.6 else float('nan')
    c=math.degrees(cy); rel=(h-c+180)%360-180 if v>0.6 else float('nan')
    print(f'{f:4d} {s:5s} {x:7.2f} {z:7.2f} {v:5.2f}  {h:8.1f}  {c:7.1f}  {rel:7.1f}')
