import sys, struct
sys.path.insert(0,'hit-and-run-web/tools')
import p3d
from pathlib import Path
def find(c,idv,out):
    if c.id==idv: out.append(c)
    for k in c.children: find(k,idv,out)
def vols(nm):
    root=p3d.read(Path(f'hit-and-run-web/source/game/art/cars/{nm}.p3d'))
    co=[]; find(root,0x7010004,co)
    res=[]
    for b in co:
        ext=struct.unpack_from('<3f',b.data)
        vs=[struct.unpack_from('<3f',k.data) for k in b.children if k.id==0x7010007]
        c,a1,a2,a3=vs
        # half-extents along axes: ext[i] along a_i
        import math
        ytop=c[1]+abs(a1[1])*ext[0]+abs(a2[1])*ext[1]+abs(a3[1])*ext[2]
        ybot=c[1]-abs(a1[1])*ext[0]-abs(a2[1])*ext[1]-abs(a3[1])*ext[2]
        # footprint extents in x,z
        hx=abs(a1[0])*ext[0]+abs(a2[0])*ext[1]+abs(a3[0])*ext[2]
        hz=abs(a1[2])*ext[0]+abs(a2[2])*ext[1]+abs(a3[2])*ext[2]
        res.append((c,ext,ytop,ybot,hx,hz))
    return res
if __name__=='__main__':
    for nm in sys.argv[1:]:
        print(nm)
        for c,ext,yt,yb,hx,hz in vols(nm):
            print('  centre(%.2f %.2f %.2f) half-ext(%.2f %.2f %.2f) ytop=%.2f ybot=%.2f footprint hx=%.2f hz=%.2f'%(*c,*ext,yt,yb,hx,hz))
