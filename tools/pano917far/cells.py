# список клеток зон дальности: python3 cells.py L [режим] -> строки "gx gz x z mode"
import os,struct,sys
ZD=os.environ.get('PANO917_ZONES','/Users/USER/Downloads/shar-psp-render-w/build/hz913')
def cells(L,modes=(2,)):
    b=open(f'{ZD}/hzzone_l{L}.bin','rb').read(); w,h=struct.unpack_from('<II',b,4); x0,z0,st=struct.unpack_from('<fff',b,12)
    out=[]
    for gz in range(h):
        for gx in range(w):
            m=b[24+gz*w+gx]
            if m in modes: out.append((gx,gz,x0+(gx+.5)*st,z0+(gz+.5)*st,m))
    return out,st
if __name__=='__main__':
    L=int(sys.argv[1]); ms=tuple(int(c) for c in sys.argv[2]) if len(sys.argv)>2 else (2,)
    c,st=cells(L,ms)
    for r in c: print(*r)
