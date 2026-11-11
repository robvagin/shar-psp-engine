"""Коллизия-96: сколько памяти займёт COL1 уровня, если держать его кольцом (страницами), а не целиком.
Читает COL1 из level.shr (смещение в SHR7 @92), раскладывает примитивы по страницам PG штук в порядке Мортона,
считает по тайлам 16x16 клеток (64 м) пик резидента для круга радиуса R. Только чтение.
Запуск: python3 tools/col_ring_measure.py путь/level.shr [PG=128]"""
import struct,sys,numpy as np
path=sys.argv[1]
f=open(path,'rb').read()
off=struct.unpack_from('<I',f,92)[0]
_,ox,oz,cell,W,H,nt,nb,nc,ns,nr,fl=struct.unpack_from('<4s3f8I',f,off)
p=off+48
TR=np.frombuffer(f,'<f4',nt*12,p).reshape(-1,12); p+=nt*48
BX=np.frombuffer(f,'<f4',nb*16,p).reshape(-1,16); p+=nb*64
CY=np.frombuffer(f,'<f4',nc*12,p).reshape(-1,12); p+=nc*48
SG=np.frombuffer(f,'<f4',ns*8,p).reshape(-1,8); p+=ns*32
cs=np.frombuffer(f,'<u4',W*H+1,p).astype(np.int64); p+=(W*H+1)*4
rf=np.frombuffer(f,'<u4',nr,p)
cnt=np.diff(cs); cellidx=np.repeat(np.arange(W*H),cnt)
typ=(rf>>29).astype(int); idx=(rf&0x1fffffff).astype(np.int64)
SZ=[48,64,48,32]
# centers in cell units
def cc(x,z): return (np.floor((x-ox)/cell).astype(int),np.floor((z-oz)/cell).astype(int))
cen=[ ( (TR[:,0]+TR[:,3]+TR[:,6])/3,(TR[:,2]+TR[:,5]+TR[:,8])/3 ),(BX[:,0],BX[:,2]),(CY[:,0],CY[:,2]),((SG[:,0]+SG[:,2])/2,(SG[:,1]+SG[:,3])/2)]
def morton(x,z):
    x=np.clip(x,0,1023).astype(np.int64);z=np.clip(z,0,1023).astype(np.int64)
    r=np.zeros_like(x)
    for b in range(10): r|=((x>>b)&1)<<(2*b)|((z>>b)&1)<<(2*b+1)
    return r
PG=int(sys.argv[2]) if len(sys.argv)>2 else 128   # prims per page
TL=16                                              # tile = 16x16 cells (64 m)
npr=[nt,nb,nc,ns]
page=[]  # per type: page index of each prim (spatial sort)
for t in range(4):
    gx,gz=cc(*cen[t]); order=np.argsort(morton(gx,gz),kind='stable'); pg=np.empty(npr[t],int); pg[order]=np.arange(npr[t])//PG; page.append(pg)
npg=[(n+PG-1)//PG for n in npr]
base=np.cumsum([0]+npg)
tile=np.zeros(W*H,int); gxs=np.tile(np.arange(W),H); gzs=np.repeat(np.arange(H),W)
TW=(W+TL-1)//TL; THh=(H+TL-1)//TL
tile=(gzs//TL)*TW+(gxs//TL)
pgid=np.array([base[t]+page[t][i] for t,i in zip([0],[0])]) if False else None
allpg=np.empty(nr,np.int64)
for t in range(4):
    m=typ==t; allpg[m]=base[t]+page[t][idx[m]]
tiles=[set() for _ in range(TW*THh)]
rtile=tile[cellidx]
tile_pg=[0]*(TW*THh); tile_refs=np.bincount(rtile,minlength=TW*THh)
for tl in range(TW*THh):
    m=rtile==tl
    s=np.unique(allpg[m]) if m.any() else np.array([],int)
    v=0
    for q in s: v|=1<<int(q)
    tile_pg[tl]=v
pgbytes=np.concatenate([np.full(npg[t],SZ[t]*PG) for t in range(4)])
def weight(v):
    # sum bytes of set bits
    b=[]; i=0
    while v:
        low=v&-v; i=low.bit_length()-1; b.append(i); v^=low
    return pgbytes[b].sum() if b else 0
full=48+nt*48+nb*64+nc*48+ns*32+(W*H+1)*4+nr*4
print(f'PG={PG} total {full/1e6:.2f} MB; pages {sum(npg)}; cellStart {(W*H+1)*4/1e6:.2f}')
for R in (200,250,280,300):
    best=0; bestparts=None
    for pz in range(0,H,12):
        for px in range(0,W,12):
            cx=(px+.5)*cell;cz=(pz+.5)*cell
            v=0;refs=0
            for tz in range(THh):
                for tx in range(TW):
                    # tile centre distance conservative: nearest point of tile to (cx,cz)
                    x0=tx*TL*cell;x1=x0+TL*cell;z0=tz*TL*cell;z1=z0+TL*cell
                    dx=max(x0-cx,0,cx-x1);dz=max(z0-cz,0,cz-z1)
                    if dx*dx+dz*dz<=R*R:
                        v|=tile_pg[tz*TW+tx];refs+=tile_refs[tz*TW+tx]
            b=weight(v)+refs*4
            if b>best: best=b;bestparts=(weight(v),refs*4)
    tot=best+(W*H+1)*4
    print(f'R={R}: ring peak prims {bestparts[0]/1e6:.2f} + refs {bestparts[1]/1e6:.2f} + cellStart resident {(W*H+1)*4/1e6:.2f} = {tot/1e6:.2f} MB; saving {(full-tot)/1e6:.2f}')
