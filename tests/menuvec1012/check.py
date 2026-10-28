#!/usr/bin/env python3
# №1012: разбор прогонов. check.py DIR_NEW DIR_OLD DIR_TOON0 DIR_MAIN_NEW DIR_MAIN_OLD — код 0, если гейт зелёный.
# По журналу (строки MENUVEC1012: рамка плашки и рамка текста, перед каждым SHOT) и по кадрам:
#   новый (Cartoon): плашка есть в каждом кадре с выбранным пунктом, она охватывает текст с запасом >= 8 px, размер не больше 300x30, внутри рамки заливка фиолетовая (R60 G28 B112, ≥ 45 % площади);
#   прежнее (файл menuvec1012old) и без Cartoon: строк MENUVEC1012 нет, внутри той же рамки заливки нет (< 5 %);
#   кроме рамки плашки кадр прежнего и нового совпадает (в полосе y >= 70: на верху отладочная строка кадра), т.е. остальное меню не тронуто.
import re, sys
from PIL import Image, ImageChops
newd, oldd, toond, mnew, mold = sys.argv[1:6]
SHOTS = ['pause_0', 'pause_1', 'opt_0', 'opt_1', 'picture_0', 'picture_1', 'picture_3', 'camera_0', 'camera_1']
ok = True
def chk(c, s):
    global ok; ok &= bool(c); print('  %s %s' % ('ok  ' if c else 'FAIL', s))
def plates(d):   # имя кадра -> последняя рамка плашки перед SHOT, или None
    res, last, cnt = {}, None, 0
    for l in open(d + '/smoke.txt', errors='replace'):
        m = re.match(r'MENUVEC1012 plate=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=(-?\d+),(-?\d+),(\d+),(\d+)', l)
        if m: last = tuple(int(x) for x in m.groups()); cnt += 1
        m = re.match(r'SHOT (\S+) ', l)
        if m: res[m.group(1)] = last; last = None
    return res, cnt
def fillfrac(img, r):
    x0, y0, x1, y1 = [max(0, v) for v in r]; px = img.load(); n = t = 0
    for y in range(y0 + 3, min(y1 - 3, img.size[1])):
        for x in range(x0 + 3, min(x1 - 3, img.size[0])):
            t += 1; R, G, B = px[x, y]
            n += abs(R - 60) < 34 and abs(G - 28) < 30 and abs(B - 112) < 38 and B > R + 20
    return n / t if t else 0.
def im(d, n): return Image.open('%s/%s.png' % (d, n)).convert('RGB')
pn, cn = plates(newd); po, co = plates(oldd); pt, ct = plates(toond)
chk(cn > 100 and co == 0 and ct == 0, 'строк MENUVEC1012: новый %d, прежний %d, без Cartoon %d' % (cn, co, ct))
for name in SHOTS:
    r = pn.get(name)
    if not r: chk(False, '%-10s плашки нет' % name); continue
    x0, y0, x1, y1, tx, ty, tw, lh = r
    wide = x0 <= tx - 8 and x1 >= tx + tw + 8 and (x1 - x0) <= 300 and (y1 - y0) <= 30 and y0 <= ty + 3 and y1 >= ty + lh - 4
    fa, fo, ft = fillfrac(im(newd, name), r[:4]), fillfrac(im(oldd, name), r[:4]), fillfrac(im(toond, name), r[:4])
    a, b = im(newd, name), im(oldd, name)
    d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 24 else 0)
    d.paste(0, (0, 0, 480, 70)); d.paste(0, (max(0, x0 - 16), max(0, y0 - 6), min(480, x1 + 16), min(272, y1 + 6)))   # рамка плашки (и блик слева) не считается
    rest = sum(d.getdata()) // 255
    chk(wide and fa >= .45 and fo < .05 and ft < .05 and rest < 150, '%-10s плашка %3dx%-2d охватывает текст %s | заливка новая %2d %% прежняя %2d %% без Cartoon %2d %% | вне плашки отличий %d px' % (name, x1 - x0, y1 - y0, wide, fa * 100, fo * 100, ft * 100, rest))
# главное меню: журнал UI до экрана меню не доходит, поэтому по кадрам — разница нового и прежнего (файл menuvec1012old) целиком в полосе названия пункта: >= 300 px, рамка не больше 300x40
d = ImageChops.difference(im(mnew, 'm0'), im(mold, 'm0')).convert('L').point(lambda v: 255 if v > 24 else 0); bb = d.getbbox(); cnt = sum(d.getdata()) // 255
chk(bb and cnt >= 300 and bb[0] >= 120 and bb[2] <= 360 and bb[1] >= 20 and bb[3] <= 70 and bb[2] - bb[0] <= 300 and bb[3] - bb[1] <= 40, 'главное меню m0 (NEW GAME): разница с прежним %d px, рамка %s — только плашка названия' % (cnt, bb))
d = ImageChops.difference(im(mnew, 'm1'), im(mold, 'm1')).convert('L').point(lambda v: 255 if v > 24 else 0)
chk(d.getbbox() is None, 'главное меню m1 (LOAD GAME, пункт недоступен — приглушён): без плашки, кадр совпадает с прежним, как задумано')
print('  зелёный: плашка у выделенного пункта во всех кадрах Cartoon; прежнее и без Cartoon без неё; остальное меню не тронуто' if ok else '  FAIL: таблица выше')
sys.exit(0 if ok else 1)
