import sys, math
rows=[l.rstrip('\n').split('\t') for l in open(sys.argv[1])][1:]
d=[(int(r[0]),float(r[1]),float(r[2]),math.degrees(math.atan2(float(r[10]),float(r[12]))),r[4]) for r in rows if len(r)>12]
w=lambda a:(a+180)%360-180
print('кадр фаза x z кам° курс(10кадр)° ход-кам°')
prev=None
for i in range(10,len(d)):
    f,x,z,c,ph=d[i]
    cut=abs(w(c-d[i-1][3]))>15
    dx,dz=x-d[i-10][1],z-d[i-10][2]; v=math.hypot(dx,dz)/0.2
    h=math.degrees(math.atan2(dx,dz)) if v>1.5 else None
    if cut or (f%40==0 and h is not None):
        print(f'{f:4d} {ph} {x:7.1f} {z:7.1f} cam={c:6.1f} course={"  -  " if h is None else f"{h:6.1f}"} rel={"-" if h is None else f"{w(h-c):6.1f}"} v={v:4.1f}{"  <-- CUT" if cut else ""}')
