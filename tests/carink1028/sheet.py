#!/usr/bin/env python3
# лист «TEST71 / стало»: sheet.py ПАПКА ВЫХОД.png — слева 30 см (TEST71), справа 80 см
import sys
from PIL import Image, ImageDraw, ImageFont
d, out = sys.argv[1:3]
try: f = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 14)
except Exception: f = ImageFont.load_default()
rows = [('az120_d45', 'сзади-сбоку сверху (120°, 4,5 м)'), ('az150_d45', 'сзади-сбоку (150°, 4,5 м)'), ('rs', 'сзади-сбоку издали'), ('side', 'сбоку'), ('top', 'сверху'), ('end', 'с торца'), ('incar', 'из машины (камера игры)'), ('foot', 'пешком у машины')]
im = Image.new('RGB', (8 + 2 * 488, 30 + len(rows) * 296), (24, 24, 28)); dr = ImageDraw.Draw(im)
dr.text((8, 6), 'TEST71 (слева, сдвиг оболочки 30 см) / стало (справа, 80 см): №1028, обводка своей машины', fill=(240, 240, 240), font=f)
for r, (t, name) in enumerate(rows):
    y = 30 + r * 296; dr.text((8, y), name, fill=(200, 200, 210), font=f)
    for c, v in enumerate(('old', 'new')): im.paste(Image.open(f'{d}/{t}_{v}.bmp').convert('RGB'), (8 + c * 488, y + 18))
im.save(out); print(out)
