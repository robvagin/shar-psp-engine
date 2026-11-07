#!/usr/bin/env python3
"""№1208в: запасная картинка экрана вылета БЕЗ рамки роликов (без ресурсов игры), вшивается в sharexc.prx: картинка Роба (assets/crash/technical_difficulties_source.png) вписана в окно,
по бокам тот же ровный зелёный, внизу справа «HOLD POWER / TO TURN OFF», внизу слева место под код (code_rect). Раскладка — как у tools/crashpic.py g09a (там же рамка с диска для набора).
   crashpic_noframe.py <source.png> <out.shx>   (рядом пишется <out>.txt со строкой code_rect)   формат .shx — viewer/kexc/crashpic.h"""
import struct, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / 'assets/fonts/pixel/bart-vs-the-space-mutants-16bit.otf'
W, H = 480, 272; WIN = (29, 23, 451, 250); BG = (143, 193, 148)
def text(img, x, y, s):
    f = ImageFont.truetype(str(FONT), 8); d = ImageDraw.Draw(img); d.fontmode = '1'
    d.text((x + 1, y + 1), s, font=f, fill=(0, 0, 0)); d.text((x, y), s, font=f, fill=(255, 255, 255)); return int(d.textlength(s, font=f))
src = Image.open(sys.argv[1]).convert('RGB'); img = Image.new('RGB', (W, H), BG)
wx0, wy0, wx1, wy1 = WIN; ww, wh = wx1 - wx0, wy1 - wy0
sc = min(ww / src.width, wh / src.height); sw, sh = int(round(src.width * sc)), int(round(src.height * sc))
img.paste(src.resize((sw, sh), Image.LANCZOS), (wx0 + (ww - sw) // 2, wy0 + (wh - sh) // 2))
hw = max(text(Image.new('RGB', (200, 20)), 0, 0, 'HOLD POWER'), text(Image.new('RGB', (200, 20)), 0, 0, 'TO TURN OFF'))
text(img, wx1 - 12 - hw, wy1 - 27, 'HOLD POWER'); text(img, wx1 - 12 - hw, wy1 - 16, 'TO TURN OFF')
q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
pal = q.getpalette()[:768]; data = bytes(q.getdata()); n = max(data) + 1
with open(sys.argv[2], 'wb') as f:
    f.write(b'SHXC' + struct.pack('<HHHH', W, H, n, 0))
    for i in range(n): f.write(bytes(pal[i * 3:i * 3 + 3]) + b'\xff')
    f.write(data)
open(str(Path(sys.argv[2]).with_suffix('.txt')), 'w').write('code_rect %d %d 120 19\n' % (wx0 + 12, wy1 - 27))
print('crashpic_noframe: %dx%d, %d цветов' % (W, H, n))
