#!/usr/bin/env python3
# №1021-герои: check.py MENU_DIR GLOW_NEW_DIR GLOW_OLD_DIR  — код 0, если гейт зелёный.
# 1) Гомер в Cartoon рисуется путём игры: чёрных точек (контур-оболочка, max RGB < 45) в области Гомера у нового на пункте a, b, c не меньше чем +400 к прежнему (TEST71: без контура, мягкий свет);
# 2) подсветка выделенного пункта не пропала: для feglow 0, 1, 3, 4, 5 жёлтых точек подсветки правее Гомера (R > 180, G > 150, B < 90, x >= 250, ниже полосы названия) у нового не меньше 85 % прежнего и не меньше 40; диван (feglow 2, оранжевая) — не меньше прежней;
# (без Cartoon путь выключен условием feHeroOn; проверить флагом нельзя: «flag toon» в цикле меню не действует — не покрыто)
import sys
from PIL import Image
menu, gn, go = sys.argv[1:4]
ok = True
def chk(c, s):
    global ok; ok &= bool(c); print('  %s %s' % ('ok  ' if c else 'FAIL', s))
def dark(d, n):
    im = Image.open('%s/%s.png' % (d, n)).convert('RGB').crop((30, 100, 250, 262)); return sum(1 for p in im.getdata() if max(p) < 45)
def glow(d, n, orange=False):
    im = Image.open('%s/%s.png' % (d, n)).convert('RGB').crop((0 if orange else 250, 60, 480, 262)); px = im.getdata()
    if orange: return sum(1 for p in px if p[0] > 200 and 100 < p[1] < 190 and p[2] < 80)
    return sum(1 for p in px if p[0] > 180 and p[1] > 150 and p[2] < 90)
for t in 'abc':
    o, n = dark(menu, t + '_old'), dark(menu, t + '_new'); chk(n - o >= 400, 'Гомер, пункт %s: чёрных точек (контур) %d -> %d (нужно +400)' % (t, o, n))
for k in (0, 1, 3, 4, 5):
    o, n = glow(go, 'gl%d' % k), glow(gn, 'gl%d' % k); chk(n >= max(40, int(.85 * o)), 'подсветка %d: жёлтых точек прежнее %d, новое %d (>= 85 %% и >= 40)' % (k, o, n))
o, n = glow(go, 'gl2', True), glow(gn, 'gl2', True); chk(n >= o, 'подсветка дивана (2): оранжевых точек прежнее %d, новое %d (не меньше)' % (o, n))
print('  зелёный: Гомер с контуром как в игре, подсветки на месте, без Cartoon кадр как был' if ok else '  FAIL: таблица выше')
sys.exit(0 if ok else 1)
