#!/usr/bin/env python3
# №1035: check.py DIR_NEW DIR_OLD — код 0, если гейт зелёный. Кадры одной сцены (новое и flag radar1035 0 = TEST72): центр радара (423,220).
# В помещении (r_house, r_kwik): внутри круга (r < 34) новое отличается от прежнего не меньше чем на 700 точек (карты нет, видна сцена), на рамке-кольце (38 <= r <= 42) — не больше 90 (кольцо осталось);
# на улице после выхода (r_street1, r_street2) и до входа (r_street0): внутри круга отличий не больше 150 (карта вернулась в тот же кадр, на улице ничего не менялось).
import sys
from PIL import Image
newd, oldd = sys.argv[1:3]
ok = True
def chk(c, s):
    global ok; ok &= bool(c); print('  %s %s' % ('ok  ' if c else 'FAIL', s))
def diff(name, rmin, rmax):
    a = Image.open('%s/%s.png' % (newd, name)).convert('RGB').load(); b = Image.open('%s/%s.png' % (oldd, name)).convert('RGB').load(); n = 0
    for y in range(170, 270):
        for x in range(375, 475):
            r2 = (x - 423) ** 2 + (y - 220) ** 2
            if r2 < rmin * rmin or r2 > rmax * rmax: continue
            if max(abs(a[x, y][i] - b[x, y][i]) for i in range(3)) > 24: n += 1
    return n
for nm in ('r_house', 'r_kwik'):
    d_in, d_ring = diff(nm, 0, 34), diff(nm, 38, 42)
    chk(d_in >= 700 and d_ring <= 90, '%-9s внутри круга отличий %4d (нужно >= 700, карты нет), на кольце %3d (нужно <= 90, кольцо осталось)' % (nm, d_in, d_ring))
for nm in ('r_street0', 'r_street1', 'r_street2'):
    d_in = diff(nm, 0, 42); chk(d_in <= 150, '%-9s на улице отличий в круге %4d (нужно <= 150: карта на месте)' % (nm, d_in))
print('  зелёный: в помещении кольцо без карты и фона, на улице карта сразу на месте' if ok else '  FAIL: таблица выше')
sys.exit(0 if ok else 1)
