import re,statistics,os,sys
from PIL import Image, ImageDraw, ImageFont
R='/Users/USER/Downloads/SHAR/03_РАБОЧИЕ_КОПИИ_агентов/shar-psp-sv-g01-cars/'
FONT='/System/Library/Fonts/Supplemental/Arial.ttf'
def F(sz): return ImageFont.truetype(FONT,sz)
l1=R+'build/abc_keep/static_L1/'; l4=R+'build/abc_keep/static_L4/'
tA=R+open(R+'build/abc_turn0.dir').read().strip()+'/'; tC=R+open(R+'build/abc_turn2.dir').read().strip()+'/'
def cost(d,sc_pref):
    out={}
    for v in range(3):
        car=[];hull=[];hc=[]
        for l in open(d+'smoke.txt',errors='replace'):
            m=re.match(r'CAR3DSUB t=\S+ sc=%s_v%d shadow=\S+ car=([\d.]+)'%(sc_pref,v),l)
            if m: car.append(float(m.group(1)))
            m=re.match(r'TOONHULL t=\S+ scene=%s_v%d per_s calls=(\d+) items=\d+ flushes=\d+ words=\d+ us=(\d+) push_us=\d+ flush_us=\d+ fps=([\d.]+)'%(sc_pref,v),l)
            if m: hull.append(int(m.group(2))/30.0); hc.append(int(m.group(1))/30.0)
        out[v]=(statistics.median(car)*1000 if car else None, statistics.median(hull) if hull else None, statistics.median(hc) if hc else None)
    return out
c1=cost(l1,'L1'); c4=cost(l4,'L4')
W,H=480,272; Z=2; PAD=14; CW=W*Z; TOP=420; HEAD=120
cols=['A  как сейчас','B  без оболочки у мелочи (<32 треуг. или <0,6 м)','C  толщина раз в 4 кадра']
rows=[('L1 день · обычное расстояние',l1,'L1n'),('L1 день · вблизи спереди',l1,'L1f'),('L1 день · вблизи сбоку',l1,'L1s'),
      ('L4 ночь · обычное расстояние',l4,'L4n'),('L4 ночь · вблизи спереди',l4,'L4f'),('L4 ночь · вблизи сбоку',l4,'L4s')]
nrows=len(rows)+3   # поворот: 3 строки
LAB=40
sheetW=PAD+3*(CW+PAD); sheetH=HEAD+nrows*(H*Z+LAB+PAD)+TOP+PAD
im=Image.new('RGB',(sheetW,sheetH),(24,24,28)); dr=ImageDraw.Draw(im)
dr.text((PAD,10),'Контур машины: варианты A | B | C  (эмулятор, одна и та же поза, снимки ×2)',font=F(34),fill=(255,255,255))
for i,c in enumerate(cols): dr.text((PAD+i*(CW+PAD),HEAD-44),c,font=F(28),fill=(255,214,102))
y=HEAD
def shot(path):
    return Image.open(path).convert('RGB').resize((W*Z,H*Z),Image.NEAREST)
for lab,d,pre in rows:
    dr.text((PAD,y+4),lab,font=F(24),fill=(200,200,210)); y+=LAB
    for v in range(3):
        p=d+f'{pre}_v{v}.png'
        if os.path.exists(p): im.paste(shot(p),(PAD+v*(CW+PAD),y))
        else: dr.text((PAD+v*(CW+PAD)+20,y+20),'нет снимка',font=F(28),fill=(255,90,90))
    y+=H*Z+PAD
# поворот: кадры 02, 05, 08 для A и C
for k,(fr) in enumerate(('t02','t05','t08')):
    dr.text((PAD,y+4),f'L1 день · поворот, кадр {fr} (0,2 с шаг): A и C; у B толщина не зависит от времени, снимок поворота не делался',font=F(24),fill=(200,200,210)); y+=LAB
    for v,dd in ((0,tA),(2,tC)):
        p=dd+f'{fr}.png'
        if os.path.exists(p): im.paste(shot(p),(PAD+v*(CW+PAD),y))
    dr.rectangle([PAD+1*(CW+PAD),y,PAD+1*(CW+PAD)+CW,y+H*Z],outline=(70,70,80)); dr.text((PAD+1*(CW+PAD)+30,y+H*Z//2-14),'B: снимок поворота не делался (контур B от времени не зависит)',font=F(22),fill=(150,150,160))
    y+=H*Z+PAD
dr.text((PAD,y+4),'Врезы ×4: фара (L1 вблизи спереди) и колесо (L4 вблизи сбоку) — смотреть толщину и наличие линии',font=F(24),fill=(200,200,210)); y+=LAB
for v in range(3):
    x0=PAD+v*(CW+PAD)
    for k,(d,pre,box) in enumerate(((l1,'L1f',(131,168,211,218)),(l4,'L4s',(277,125,357,175)))):
        p=d+f'{pre}_v{v}.png'
        if os.path.exists(p):
            c=Image.open(p).convert('RGB').crop(box).resize((320,200),Image.NEAREST); im.paste(c,(x0+k*(320+10),y))
y+=200+PAD
def fmt(c):
    car,hull,hc=c
    if car is None: return 'нет замера'
    return f'машина {car:.0f} мкс эмулятора ≈ {car*5.8/1000:.2f} мс PSP;  оболочки {hc:.0f} вызовов, {hull:.0f} мкс ≈ {hull*5.8/1000:.2f} мс PSP'
dr.text((PAD,y+6),'Цена кадра машины игрока (медиана за стоящий кадр, мкс эмулятора, ×5,8 — на PSP). Эмулятор, оценка; PSP не мерялось. Шум замера на загруженном Маке ±15 %, поэтому надёжно только число вызовов оболочек.',font=F(26),fill=(255,214,102)); y+=44
for lab,cc in (('L1 день',c1),('L4 ночь',c4)):
    for v in range(3):
        dr.text((PAD+v*(CW+PAD),y),f'{lab}: {fmt(cc[v])}',font=F(21),fill=(255,255,255))
    y+=34
im=im.crop((0,0,sheetW,y+PAD))
os.makedirs(os.path.expanduser('~/Desktop/SHAR_контур_машины'),exist_ok=True)
out=os.path.expanduser('~/Desktop/SHAR_контур_машины/варианты_ABC.png'); im.save(out); print(out,im.size)
print('L1',c1); print('L4',c4)
