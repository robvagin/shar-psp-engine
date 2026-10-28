#!/usr/bin/env python3
# №1012: лист «TEST71 / стало». sheet.py OUT_DIR OUT.png — пары кадров: прежнее (файл menuvec1012old, как TEST71) слева, новое справа
import sys
from PIL import Image, ImageDraw
O = sys.argv[1]
rows = [('PAUSE', 'out_a1', 'out_a2', 'pause_1', (100, 56, 380, 252)), ('OPTIONS', 'out_a1', 'out_a2', 'opt_1', (100, 56, 380, 200)), ('SOUND sliders', 'out_a1', 'out_a2', 'picture_3', (100, 56, 380, 252)),
        ('CAMERA', 'out_a1', 'out_a2', 'camera_1', (80, 56, 400, 150)), ('MAIN MENU', 'out_a4', 'out_a5', 'm0', (0, 0, 480, 272))]
T = [(l, Image.open('%s/%s/%s.png' % (O, o, s)).convert('RGB').crop(b), Image.open('%s/%s/%s.png' % (O, n, s)).convert('RGB').crop(b)) for l, n, o, s, b in rows]
im = Image.new('RGB', (max(t[1].size[0] for t in T) * 2 + 20, sum(t[1].size[1] + 18 for t in T)), (30, 30, 30)); y = 0; d = ImageDraw.Draw(im)
for l, b, a in T:
    d.text((4, y + 3), '%s: TEST71 (left) / new (right)' % l, fill=(255, 255, 0)); y += 18; im.paste(b, (0, y)); im.paste(a, (b.size[0] + 20, y)); y += b.size[1]
im.save(sys.argv[2]); print(sys.argv[2], im.size)
