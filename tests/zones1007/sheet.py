#!/usr/bin/env python3
# лист «TEST71 / стало»: слева кадр без сайдкара (как TEST71), справа с zones1007p.bin; sheet.py НОВЫЙ СТАРЫЙ ВЫХОД.png
import sys, os
from PIL import Image, ImageDraw, ImageFont
new, old, out = sys.argv[1:4]
names = [('L1_top', 'L1 парковка Квик-и-Марта, сверху'), ('L1_side', 'L1 парковка сбоку'), ('L1_foot', 'L1 Гомер на парковке'), ('L2_top', 'L2 площадка у асфальта'), ('L4_top', 'L4 парковка'), ('L5_top', 'L5 площадка'), ('L7_top', 'L7 парковка')]
names = [(n, t) for n, t in names if os.path.exists(f'{new}/{n}.bmp') and os.path.exists(f'{old}/{n}.bmp')]
W, H = 480, 272; pad = 8; top = 34
try: f = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 15); f2 = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 12)
except Exception: f = f2 = ImageFont.load_default()
cols = 2; rows = len(names)
im = Image.new('RGB', (pad + cols * (W + pad), top + rows * (H + 22 + pad)), (24, 24, 28)); d = ImageDraw.Draw(im)
d.text((pad, 8), 'TEST71 (слева) / стало (справа): №1007б, тон площадок у асфальта по куску', fill=(240, 240, 240), font=f)
for r, (n, t) in enumerate(names):
    y = top + r * (H + 22 + pad)
    d.text((pad, y), t, fill=(200, 200, 210), font=f2)
    for c, dd in enumerate((old, new)):
        im.paste(Image.open(f'{dd}/{n}.bmp').convert('RGB'), (pad + c * (W + pad), y + 18))
im.save(out); print(out, len(names), 'мест')
