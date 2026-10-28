#!/usr/bin/env python3
# лист «TEST71 / стало» главного меню: sheet.py ПАПКА_ПРОГОНА ВЫХОД.png — слева прежнее (flag fehero1021 0), справа новое; пункты NEW GAME (a), следующий (b), следующий (c), подсветка дивана
import sys
from PIL import Image, ImageDraw
d, out = sys.argv[1:3]
rows = [('NEW GAME', 'a_old', 'a_new'), ('carousel item 2', 'b_old', 'b_new'), ('carousel item 3', 'c_old', 'c_new')]
im = Image.new('RGB', (8 + 2 * 488, 4 + 3 * 292), (24, 24, 28)); dr = ImageDraw.Draw(im); y = 4
for lab, o, n in rows:
    dr.text((8, y), '%s: TEST71 (left) / new (right)' % lab, fill=(240, 240, 120)); y += 14
    im.paste(Image.open('%s/%s.png' % (d, o)).convert('RGB'), (8, y)); im.paste(Image.open('%s/%s.png' % (d, n)).convert('RGB'), (8 + 488, y)); y += 278
im.save(out); print(out, im.size)
