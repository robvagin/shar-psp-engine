import struct, sys, math
def load(path):
    d=open(path,'rb').read()
    nRoad,offRoad,nPt,offPt=struct.unpack_from('<4I',d,4*8+0)[0:4] if False else (None,)*4
    # TrHeader: char[4] magic, u32 version,size, nJunc,offJunc,nRoad,offRoad,nPt,offPt,...
    v=struct.unpack_from('<4s11I',d,0); nJ,offJ,nR,offR,nP,offP=v[3],v[4],v[5],v[6],v[7],v[8]
    pts=[struct.unpack_from('<4f',d,offP+16*i) for i in range(nP)]
    roads=[]
    for r in range(nR):
        sj,ej,rev,lanes,fl,mc,sp,_,fp,np_,_,ln,wd,_=struct.unpack_from('<4H4BI2H3f',d,offR+32*r)
        roads.append(dict(lanes=lanes,flags=fl,first=fp,n=np_,width=wd))
    return pts,roads
def segs(pts,roads,use_shortcut=False):
    S=[]
    for r in roads:
        if (r['flags']&1) and not use_shortcut: continue
        for k in range(max(1,r['lanes'])):
            P=pts[r['first']+k*r['n']:r['first']+(k+1)*r['n']]
            for a,b in zip(P,P[1:]): S.append((a[0],a[2],b[0],b[2],r['width']))
    return S
def dist(S,x,z):
    best=1e9; bw=0
    for ax,az,bx,bz,w in S:
        dx,dz=bx-ax,bz-az; L=dx*dx+dz*dz
        t=0 if L<1e-9 else max(0,min(1,((x-ax)*dx+(z-az)*dz)/L)); px,pz=ax+t*dx,az+t*dz
        d=math.hypot(x-px,z-pz)
        if d<best: best=d; bw=w
    return best,bw
if __name__=='__main__':
    pts,roads=load(sys.argv[1]); S=segs(pts,roads); print('дорог',len(roads),'отрезков полос',len(S),'ширины',sorted(set(round(r['width'],1) for r in roads))[:8])
    tsv=sys.argv[2]; rows=[l.rstrip('\n').split('\t') for l in open(tsv)]; h=rows[0]; ix={k:i for i,k in enumerate(h)}
    prev=None; off=0; tot=0; mx=0
    for r in rows[1:]:
        px,pz=float(r[ix['pol_x']]),float(r[ix['pol_z']])
        if px==0 and pz==0: continue
        pz=-pz   # ось z оригинала = −z порта
        d,w=dist(S,px,pz); tot+=1; mx=max(mx,d); 
        if d>max(w,6)/2+2: off+=1
    print('кадров полиции',tot,'вне дороги (>w/2+2)',off,'макс отклонение от оси %.1f м'%mx)
