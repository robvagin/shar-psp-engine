# сценарий съёмки панорамы: по клеткам (gx,gz) уровня L; глаз — мировая высота «земля клетки + 2» из ленты horizon.shr (ey клетки), 16 направлений × углы места, узкий кадр FOV (вертикаль, град)
# python3 gen_cap.py L ИМЯ gx,gz[;gx,gz...] [FOV] [PITCHES] -> ИМЯ.txt и ИМЯ.json (мировая высота глаза по клетке); кадры p_<gx>_<gz>_<k>_<j>
import os,sys,math,struct,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from cells import cells
SET=os.environ.get('PANO917_SET','/Users/USER/Downloads/shar-psp-integ/build/test57tour/PSP/GAME/SHARTEST15')
def horizon_ey(L):
    b=open(f'{SET}/horizon.shr' if L==1 else f'{SET}/l{L}/horizon.shr','rb').read(); n=struct.unpack_from('<I',b,4)[0]
    return [struct.unpack_from('<3f',b,32+32*i) for i in range(n)]
if __name__=='__main__':
    L=int(sys.argv[1]); name=sys.argv[2]; sel=[tuple(map(int,t.split(','))) for t in sys.argv[3].split(';')]
    fov=float(sys.argv[4]) if len(sys.argv)>4 else 20.0; pit=[float(v) for v in (sys.argv[5] if len(sys.argv)>5 else '0,20,40').split(',')]
    NAZ=16; allc,_=cells(L,tuple(range(256))); cm={(c[0],c[1]):c for c in allc}; hz=horizon_ey(L); meta={}
    S=["wait 12","scene LOAD",f"flag golevel {L}","wait 40","flag cfgdefaults 1","flag far150 0","flag drawdist 100","press triangle 0.2","wait 3"]
    for gx,gz in sel:
        _,_,x,z,m=cm[(gx,gz)]
        h=min(hz,key=lambda c:(c[0]-x)**2+(c[1]-z)**2); assert abs(h[0]-x)<1 and abs(h[1]-z)<1,(L,gx,gz,h)
        ey=h[2]; meta[f'{gx}_{gz}']=dict(x=x,z=z,ey=ey,mode=m)
        S+=[f"foot {x:.0f} {z:.0f} 0","wait 4","flag pano917cap 150 250"]
        for k in range(NAZ):
            a=2*math.pi*k/NAZ
            for j,p in enumerate(pit):
                pr=math.radians(p); d=(math.sin(a)*math.cos(pr),math.sin(pr),math.cos(a)*math.cos(pr))
                S+=[f"cam {x:.0f} {ey:.2f} {z:.0f} {d[0]:.5f} {d[1]:.5f} {d[2]:.5f} {fov:g}","wait 0.35",f"shot p_{gx}_{gz}_{k}_{j}","wait 0.1"]
        S+=["flag pano917cap 0","cam off"]
    S.append("end"); open(name+'.txt','w').write("\n".join(S)+"\n"); json.dump(meta,open(name+'.json','w'))
