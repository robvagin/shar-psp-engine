# python3 mkpano2.py L R -> data/[l<L>/]pano917.bin из кадров out_cap<L> (p_<gx>_<gz>_<k>_<j>.png), R — радиус кольца в заголовке
import sys,os,json
sys.path.insert(0,'tools')
import pano
from PIL import Image
L=int(sys.argv[1]); R=float(sys.argv[2]); H=128; PSM=4
d=f'out_cap{L}'; meta=json.load(open(f'sc/L{L}.json')); items=[]
for k,m in sorted(meta.items(),key=lambda kv:(kv[1]['z'],kv[1]['x'])):
    gx,gz=map(int,k.split('_'))
    p=pano.build(gx,gz,d,20,[0,20,40],H,PSM); p.update(x=m['x'],z=m['z'],ey=m['ey'],gx=gx,gz=gz,mode=m['mode'])
    os.makedirs('rings',exist_ok=True); Image.fromarray(pano.decode(p)).save(f'rings/L{L}_{gx}_{gz}.png'); p.pop('rgba'); p.pop('idx'); items.append(p)
    print(L,gx,gz,'lo %.1f hi %.1f punch %s'%(p['lo'],p['hi'],p['punch']))
out='data'+('' if L==1 else f'/l{L}'); os.makedirs(out,exist_ok=True)
pano.write(f'{out}/pano917.bin',items,R,60.0,H,PSM); print(out,os.path.getsize(out+'/pano917.bin'))
