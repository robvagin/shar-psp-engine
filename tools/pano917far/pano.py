# панорамы клеток: кадры бота -> кольцо (16 плоских сегментов по 128 текселей с полями) -> CLUT4 -> атлас 512 × (4·H) в swizzle T4 -> pano917.bin уровня
# формат 'PNO1': u32 n, u32 SW=128, u32 H, f32 R, f32 step, 12 байт резерв → 32 байта; n записей по 32 байта: f32 x, z, eyeY(не используется), emin, emax (градусы), u32 off, u32 size, u32 резерв; затем блобы: CLUT 16×RGBA8888 (64 байта) + атлас T4 swizzle
import numpy as np, math, struct, os
from PIL import Image
NAZ=16; ASP=480/272; SW=128; DEL=2*math.pi/NAZ
def load(path):
    a=np.array(Image.open(path).convert('RGB')).astype(np.float32)
    pm=(a[...,0]<=6)&(a[...,1]>=248)&(a[...,2]<=6)   # маркер пробивки (1,254,1)
    key=(a[...,0]-a[...,1]>90)&(a[...,2]-a[...,1]>90)&(a[...,0]>150)&(a[...,2]>150)   # №917б: пурпур с запасом (дизеринг, примесь на кромке)
    k2=key.copy()
    for ax in (0,1):
        for sh in (-1,1): k2|=np.roll(key,sh,axis=ax)   # кромка на тексель внутрь: без розового ободка
    return a,k2|pm,pm
def loadset(d,gx,gz,npit):
    # d — каталог или список каталогов по приоритету (пересъёмка нижнего угла места с землёй лежит в первом, верхние углы — из прежнего)
    ds=d if isinstance(d,(list,tuple)) else [d]; fr={}
    for k in range(NAZ):
        for j in range(npit):
            for dd in ds:
                f=f'{dd}/p_{gx}_{gz}_{k}_{j}.png'
                if os.path.exists(f): fr[(k,j)]=load(f); break
            else: raise FileNotFoundError(f'нет кадра p_{gx}_{gz}_{k}_{j} в {ds}')
    return fr
def segcols(ss=1,ssoff=0):
    # азимут и cos φ столбцов сегмента: 128 текселей, c=1 ↔ левый край, c=126 ↔ правый; плоский квад: x=tan φ линейно по u
    c=np.arange(SW)+(ssoff+.5)/ss-.5
    t=(c-1)/125.0; x=(2*t-1)*math.tan(DEL/2); phi=np.arctan(x); return phi
def sample(frames,fov,pitches,H,emin,emax,ss=3):
    tv=math.tan(math.radians(fov)/2); th=tv*ASP
    ymax=math.tan(math.radians(emax)); ymin=math.tan(math.radians(emin))
    out=np.zeros((NAZ,H,SW,5),np.float32)
    pc=np.array(pitches,np.float32)
    for k in range(NAZ):
        mid=(k+.5)*DEL            # середина сегмента k: сегмент k охватывает [k·Δ, (k+1)·Δ]
        for sj in range(ss):
            for si in range(ss):
                phi=segcols(ss,si)                    # (SW,)
                az=mid+phi
                rows=np.arange(H)+(sj+.5)/ss-.5
                Y=ymax-(rows/(H-1))*(ymax-ymin)       # линейная высота на плоскости сегмента (в единицах радиуса)
                TT=Y[:,None]*np.cos(phi)[None,:]      # tan угла места
                AZ=np.broadcast_to(az[None,:],TT.shape)
                D=np.stack([np.sin(AZ),TT,np.cos(AZ)],-1)
                K=np.round(AZ/DEL-.5+.5).astype(int)%NAZ   # ближайшее направление съёмки: k·Δ
                EL=np.degrees(np.arctan(TT)); J=np.abs(EL[...,None]-pc).argmin(-1)
                for kk in np.unique(K):
                    a=kk*DEL
                    for j,p in enumerate(pitches):
                        m=(K==kk)&(J==j)
                        if not m.any(): continue
                        pr=math.radians(p)
                        f=np.array([math.sin(a)*math.cos(pr),math.sin(pr),math.cos(a)*math.cos(pr)])
                        r=-np.array([math.cos(a),0,-math.sin(a)])
                        u=np.array([-math.sin(a)*math.sin(pr),math.cos(pr),-math.cos(a)*math.sin(pr)])
                        d=D[m]; zc=d@f; xc=d@r; yc=d@u
                        ok=zc>1e-4; zs=np.where(ok,zc,1)
                        px=(xc/zs/th+1)*.5*480-.5; py=(1-yc/zs/tv)*.5*272-.5
                        ins=ok&(px>=0)&(px<=479)&(py>=0)&(py<=271)
                        rgb,key,pmk=frames[(int(kk),j)]
                        ix=np.clip(np.round(px).astype(int),0,479); iy=np.clip(np.round(py).astype(int),0,271)
                        op=(ins&~key[iy,ix]).astype(np.float32); c=rgb[iy,ix]
                        idx=np.where(m)
                        out[k][idx[0],idx[1],:3]+=c*op[:,None]; out[k][idx[0],idx[1],3]+=op; out[k][idx[0],idx[1],4]+=(ins&pmk[iy,ix]).astype(np.float32)
    n=out[...,3:4]; rgb=np.where(n>0,out[...,:3]/np.maximum(n,1e-6),0)
    return np.concatenate([rgb,n/(ss*ss),out[...,4:5]/(ss*ss)],-1)          # (NAZ,H,SW,5): цвет, доля непрозрачных, доля пробивки
def extent(frames,fov,pitches):
    # вертикальный размах содержимого: грубый проход на полном диапазоне
    H=96; r=sample(frames,fov,pitches,H,-10,50,ss=1); op=(r[...,3]>.5)|(r[...,4]>.5)
    rows=np.where(op.any(axis=(0,2)))[0]
    ymax=math.tan(math.radians(50)); ymin=math.tan(math.radians(-10))
    if len(rows)==0: return -2.0,12.0
    Y=lambda i: ymax-(i/(H-1))*(ymax-ymin)
    hi=math.degrees(math.atan(Y(max(rows[0]-1,0)))); lo=math.degrees(math.atan(Y(min(rows[-1]+1,H-1))))
    return lo,hi
def quant(r,ncol=15,iters=8,punch=False):
    # палитра: медианный разрез + k-means; индекс 0 — прозрачный; при пробивке последний индекс (15 / 255) — пробивка, альфа CLUT строго 1; размер CLUT ncol+1; цветов ncol-1 при пробивке
    rgb=r[...,:3]; op=r[...,3]>.5; pk=(r[...,4]>.5)&~op if r.shape[-1]>4 else np.zeros_like(op); px=rgb[op]; n=ncol+1; nc=ncol-1 if punch else ncol
    clut=np.zeros((n,4),np.uint8)
    if len(px)==0:
        idx=np.zeros(r.shape[:-1],np.uint8)
        if punch: idx[pk]=n-1; clut[n-1,3]=1
        return idx,clut
    sel=px if len(px)<80000 else px[np.random.RandomState(1).choice(len(px),80000,replace=False)]
    im=Image.fromarray(sel.astype(np.uint8).reshape(-1,1,3)); q=im.quantize(colors=nc,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    pal=np.array(q.getpalette()[:3*nc],np.float32).reshape(-1,3)[:nc]
    for _ in range(iters):
        a=np.zeros(len(sel),int)
        for s0 in range(0,len(sel),20000): a[s0:s0+20000]=((sel[s0:s0+20000,None,:]-pal[None])**2).sum(-1).argmin(-1)
        for c in range(len(pal)):
            m=a==c
            if m.any(): pal[c]=sel[m].mean(0)
    flat=rgb.reshape(-1,3); best=np.zeros(len(flat),np.uint8)
    for s0 in range(0,len(flat),100000): best[s0:s0+100000]=((flat[s0:s0+100000,None,:]-pal[None])**2).sum(-1).argmin(-1)+1
    idx=np.where(op,best.reshape(op.shape),0).astype(np.uint8)
    clut[1:1+len(pal),:3]=np.round(pal).astype(np.uint8); clut[1:1+len(pal),3]=255
    clut[0,:3]=np.round(px.mean(0)).astype(np.uint8)
    if punch:
        # пробивка расширена на тексель наружу (лента не оставляет полоску), цвет не затирает
        pm=pk.copy()
        for ax in (1,2):
            for sh in (-1,1): pm=pm|np.roll(pm,sh,axis=ax)
        pm&=(idx==0); idx[pm]=n-1; clut[n-1]=(0,0,0,1)
    return idx,clut
def atlas(idx,H):
    # idx (NAZ,H,SW) -> плоский (4H, 512) индексов; сегмент k: столбец (k%4)·128, строка (k//4)·H
    A=np.zeros((4*H,512),np.uint8)
    for k in range(NAZ): A[(k//4)*H:(k//4+1)*H,(k%4)*SW:(k%4+1)*SW]=idx[k]
    return A
def swz4(A):
    h,w=A.shape; b=(A[:,0::2]&15)|((A[:,1::2]&15)<<4)     # байты: 256 на строку
    bw=w//2; assert bw%16==0 and h%8==0
    t=b.reshape(h//8,8,bw//16,16).transpose(0,2,1,3)    # (блок-строка, блок-столбец, 8 строк, 16 байт)
    return np.ascontiguousarray(t).tobytes()
def swz8(A):
    h,w=A.shape; assert w%16==0 and h%8==0
    t=A.reshape(h//8,8,w//16,16).transpose(0,2,1,3); return np.ascontiguousarray(t).tobytes()
def build(gx,gz,d,fov,pitches,H,psm=4):
    fr=loadset(d,gx,gz,len(pitches))
    lo,hi=extent(fr,fov,pitches)
    if hi-lo<14: c=(hi+lo)/2; lo,hi=c-7,c+7
    lo=max(lo,-12); hi=min(hi,52)
    r=sample(fr,fov,pitches,H,lo,hi,ss=3)
    pun=bool((r[...,4]>.5).any()); idx,clut=quant(r,15 if psm==4 else 255,punch=pun)
    return dict(lo=lo,hi=hi,idx=idx,clut=clut,rgba=r,H=H,psm=psm,punch=pun,blob=clut.tobytes()+(swz4(atlas(idx,H)) if psm==4 else swz8(atlas(idx,H))))
def decode(p):
    # обратно в картинку кольца (для проверки): индексы -> цвета, непрозрачность
    idx=p['idx']; pi=len(p['clut'])-1; pu=(idx==pi)&(p['clut'][pi,3]==1); idx=np.where(pu,0,idx); c=p['clut'][idx][...,:3]; a=(idx>0)
    img=np.where(a[...,None],c,[255,0,255]); img=np.where(pu[...,None],[0,255,0],img)   # пробивка — зелёным
    ring=np.concatenate([img[k,:,1:127] for k in range(NAZ)],1)
    return ring.astype(np.uint8)
def write(path,items,R,step,H,psm=4):
    flags=1 if any(it.get('punch') for it in items) else 0   # бит 0: в картинках есть индекс пробивки (последний индекс палитры, альфа CLUT 1)
    hdr=struct.pack('<4sIIIffII',b'PNO1',len(items),SW,H,R,step,psm,flags); assert len(hdr)==32   # смещение 24: psm, 28: флаги (4 — T4/CLUT16, 8 — T8/CLUT256; 0 в старых файлах = 4)
    off=(32+32*len(items)+63)//64*64; tab=b''; blobs=b''   # blob'ы с выравниванием 64 (GE, кэш); размер блоба 64+512·4H/2 кратен 64 сам
    for it in items:
        assert len(it['blob'])%64==0
        tab+=struct.pack('<fffffIII',it['x'],it['z'],it['ey'],it['lo'],it['hi'],off+len(blobs),len(it['blob']),0); blobs+=it['blob']
    pad=b'\0'*(off-32-len(tab)-0)
    open(path,'wb').write(hdr+tab+pad+blobs)
