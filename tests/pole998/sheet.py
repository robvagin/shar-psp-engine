#!/usr/bin/env python3
# №998: лист кадров падения. sheet.py OUT_DIR_NEW OUT_DIR_OLD OUT.png — на каждый столб строка: 5 кадров (удар, +0,7, +1,7, +3,2, +5,2 с), сверху новый код, ниже красный (прежний)
import sys
from PIL import Image, ImageDraw
POLES = [204, 205, 207, 203, 214, 218, 92, 95, 96, 97, 98]; W, H = 240, 136
def load(d, i, s):
    try: return Image.open('%s/p%d_%s.png' % (d, i, s)).convert('RGB').resize((W, H))
    except Exception: return Image.new('RGB', (W, H), (40, 0, 0))
rows = [(i, d, tag) for i in POLES for d, tag in ((sys.argv[1], 'стало'), (sys.argv[2], 'было'))]
im = Image.new('RGB', (W * 5, H * len(rows)))
for r, (i, d, tag) in enumerate(rows):
    for c, s in enumerate('abcde'): im.paste(load(d, i, s), (c * W, r * H))
    ImageDraw.Draw(im).text((4, r * H + 4), '#%d %s' % (i, tag), fill=(255, 255, 0))
im.save(sys.argv[3]); print(sys.argv[3], im.size)
