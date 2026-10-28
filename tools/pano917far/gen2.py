import sys,math,json,os
sys.path.insert(0,'tools')
os.environ['PANO917_SET']=os.path.expanduser('~/Downloads/shar-psp-integ/build/test57tour/PSP/GAME/SHARTEST15')
from gen_cap import horizon_ey
from cells import cells
PIT=(0,20,40); FOV=20; CH=6
PILOT={6:[(6,7),(7,7),(7,8),(14,3),(14,4)],1:[(12,10),(12,11),(12,12),(12,13)],3:[(10,6),(10,5)]}
EXTRA_XZ={3:[(-450,-30),(-390,-30)],6:[(-450,-30),(-390,-30)]}   # набережная C7-6 / причал (G59_PIER, -417,-50)
RANK=[6,3,1,2,4,5,7]
manifest=[]; allmeta={}
for L in RANK:
    allc,_=cells(L,tuple(range(256))); cm={(c[0],c[1]):c for c in allc}; hz=horizon_ey(L)
    z2,_=cells(L,(2,)); sel=[(c[0],c[1]) for c in z2]
    for xz in EXTRA_XZ.get(L,[]):
        c=min(allc,key=lambda r:(r[2]-xz[0])**2+(r[3]-xz[1])**2); sel.append((c[0],c[1]))
    pil=set(PILOT.get(L,[])); sel=[s for s in dict.fromkeys(sel) if s not in pil]
    sel.sort(key=lambda s:(s[1],s[0]))
    for i in range(0,len(sel),CH):
        part=sel[i:i+CH]; name=f'N{L}_{i//CH}'; meta={}
        S=["scene C_BOOT","flag cfgdefaults 1","wait 6","scene L%d"%L,"flag golevel %d"%L,"wait 40","flag stop","flag player","wait 2"]
        for gx,gz in part:
            _,_,x,z,m=cm[(gx,gz)]
            h=min(hz,key=lambda c:(c[0]-x)**2+(c[1]-z)**2); assert abs(h[0]-x)<1 and abs(h[1]-z)<1,(L,gx,gz,h)
            ey=h[2]; meta[f'{gx}_{gz}']=dict(x=x,z=z,ey=ey,mode=m)
            S+=[f"scene S{gx}_{gz}",f"foot {x:.0f} {z:.0f} 0","wait 6","flag pano917cap 200 600","wait 4"]
            for k in range(16):
                a=2*math.pi*k/16
                for j,p in enumerate(PIT):
                    pr=math.radians(p); d=(math.sin(a)*math.cos(pr),math.sin(pr),math.cos(a)*math.cos(pr))
                    S+=[f"cam {x:.0f} {ey:.2f} {z:.0f} {d[0]:.5f} {d[1]:.5f} {d[2]:.5f} {FOV}","wait 0.5",f"shot p_{gx}_{gz}_{k}_{j}","wait 0.1"]
            S+=["flag pano917cap 0","cam off"]
        S.append("end"); open(f'sc/{name}.txt','w').write("\n".join(S)+"\n"); json.dump(meta,open(f'sc/{name}.json','w')); manifest.append(name)
    print(L,len(sel),'новых клеток (без пилота)')
open('sc/manifest.txt','w').write("\n".join(manifest)+"\n"); print(len(manifest),'сценариев')
