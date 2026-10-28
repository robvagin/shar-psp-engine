import math,sys
rows=[l.rstrip('\n').split('\t') for l in open(sys.argv[1])][1:]
lo=int(sys.argv[2]); hi=int(sys.argv[3])
d=[(int(r[0]),float(r[2]),float(r[4]),math.degrees(math.atan2(float(r[10]),float(r[12])))) for r in rows]
w=lambda a:(a+180)%360-180
out=[]
for i in range(20,len(d)):
    f,x,z,c=d[i]
    if not lo<=f<=hi: continue
    p=d[i-10]; dt=(f-p[0])/50; v=math.hypot(x-p[1],z-p[2])/dt
    om=w(c-p[3])/dt
    q=d[i-10]; h=math.degrees(math.atan2(x-q[1],z-q[2])); rel=w(h-c)
    out.append((f,v,om,rel))
for k in range(0,len(out),max(1,len(out)//6)):
    f,v,om,rel=out[k]; print(f'  кадр {f} v={v:4.1f} ω={om:6.1f}°/с ход−вид={rel:6.1f}')
vs=[o[1] for o in out if o[1]>1.5]; oms=[abs(o[2]) for o in out if o[1]>1.5]
print('  среднее v=%.2f ω=%.1f'%(sum(vs)/len(vs),sum(oms)/len(oms)) if vs else '  нет хода')
