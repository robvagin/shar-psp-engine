import sys,os,numpy as np,collections
R=os.getcwd(); sys.path[:0]=[R+'/tools']
import shr7read as S
def analyse(path,tex):
    D=S.load(path); P=D['P']; T=D['TEX']; C=D['CELL']
    e1=P[:,1]-P[:,0]; e2=P[:,2]-P[:,0]; n=np.cross(e1,e2); ln=np.linalg.norm(n,axis=1); ok=ln>1e-9
    nn=np.zeros_like(n); nn[ok]=n[ok]/ln[ok,None]; cen=P.mean(axis=1)
    gaps=[]; wi=np.where(np.isin(T,tex))[0]
    # пространственная сетка по центрам, 8 м
    key=np.floor(cen/8).astype(int); grid=collections.defaultdict(list)
    for i,k in enumerate(map(tuple,key)): grid[k].append(i)
    for i in wi:
        k=tuple(key[i]); best=None
        for dx in(-1,0,1):
            for dy in(-1,0,1):
                for dz in(-1,0,1):
                    for j in grid.get((k[0]+dx,k[1]+dy,k[2]+dz),()):
                        if j==i or T[j] in tex: continue
                        if abs(nn[i]@nn[j])<0.995: continue
                        g=abs((cen[i]-P[j,0])@nn[j])
                        if g>0.6: continue
                        # центр окна внутри треугольника j (барицентрически, в плоскости)
                        a,b,c=P[j]; v0=b-a; v1=c-a; v2=cen[i]-a
                        d00=v0@v0; d01=v0@v1; d11=v1@v1; d20=v2@v0; d21=v2@v1; den=d00*d11-d01*d01
                        if den<1e-12: continue
                        u=(d11*d20-d01*d21)/den; w=(d00*d21-d01*d20)/den
                        if u>=-0.05 and w>=-0.05 and u+w<=1.05:
                            if best is None or g<best: best=g
        gaps.append(best)
    return wi,gaps
def flick_dist(gap,n=1.5,f=200.): return (gap*f*n*65535/(f-n))**.5
if __name__=='__main__':
    L=int(sys.argv[1]); tex=[int(x) for x in sys.argv[2].split(',')]
    p=os.path.expanduser('~/Downloads/shar-psp-integ/build/test83/PSP/GAME/SHARTEST15/'+('' if L==1 else f'l{L}/')+'level.shr')
    wi,g=analyse(p,tex); have=[x for x in g if x is not None]
    print('window tris',len(wi),'with wall behind',len(have))
    q=np.quantile(have,[0,.1,.25,.5,.75,.9,1]); print('gap m quantiles',q.round(4))
    print('flicker distance m at those gaps (n=1.5,f=200)',[round(flick_dist(x),1) for x in q])
    h=np.histogram(have,bins=[0,.002,.01,.02,.05,.1,.2,.4,.61])[0]; print('hist [0,2mm,1cm,2cm,5cm,10cm,20cm,40cm,60cm]',h)
