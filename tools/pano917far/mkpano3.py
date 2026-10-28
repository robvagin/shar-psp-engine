# python3 mkpano3.py L R [ВЫВОД] — все клетки уровня: пилот (out_cap<L>, sc/L<L>.json) + пачки (out_cap_N<L>_<i>, sc/N<L>_<i>.json)
import sys,os,json,glob
sys.path.insert(0,'tools')
import pano
from PIL import Image
L=int(sys.argv[1]); R=float(sys.argv[2]); OUTB=sys.argv[3] if len(sys.argv)>3 else 'data2'; H=128; PSM=4
srcs=[]
if os.path.exists(f'sc/L{L}.json'): srcs.append((f'out_cap{L}',f'sc/L{L}.json'))
for js in sorted(glob.glob(f'sc/N{L}_*.json')): srcs.append((f'out_cap_{os.path.basename(js)[:-5]}',js))
items=[]; miss=0
for d,js in srcs:
    for k,m in json.load(open(js)).items():
        gx,gz=map(int,k.split('_'))
        if not os.path.exists(f'{d}/p_{gx}_{gz}_15_2.png'): miss+=1; print('нет кадров',d,k); continue
        p=pano.build(gx,gz,d,20,[0,20,40],H,PSM); p.update(x=m['x'],z=m['z'],ey=m['ey'],gx=gx,gz=gz,mode=m['mode'])
        os.makedirs('rings',exist_ok=True); Image.fromarray(pano.decode(p)).save(f'rings/L{L}_{gx}_{gz}.png'); p.pop('rgba'); p.pop('idx'); items.append(p)
items.sort(key=lambda p:(p['z'],p['x']))
out=OUTB+('' if L==1 else f'/l{L}'); os.makedirs(out,exist_ok=True)
pano.write(f'{out}/pano917.bin',items,R,60.0,H,PSM); print(f'L{L}: панорам {len(items)}, не хватает {miss}, файл {os.path.getsize(out+"/pano917.bin")} байт')
