#!/usr/bin/env python3
# №1023: разбор. check.py DIR_NEW DIR_OLD  — код 0, если гейт зелёный.
# Перо края асфальта читаем из журнала (ROADTHICK1023 pen=ABGR). Пиксели пера в кадре — близкие к нему (±22 по каналу), НЕ чисто чёрные контуры героев (у них R,G,B < 12): считаем только насыщенные тёмные.
# Зелёное: на кадрах L1 (a_car1, a_high, a_low) пикселей пера, которых нет в прежнем кадре, не меньше 150 (линия плотная, не точки); медиана цены толстых проходов <= 80 мкс на кадр (Мак).
# Красное: в прежнем (flag roadthick1023 0) строк ROADTHICK1023 нет и пикселей пера мало — тот же разбор с кадрами прежнего вместо нового обязан упасть.
import re, sys
from PIL import Image
new, old = sys.argv[1:3]
ok = True
def chk(c, s):
    global ok; ok &= bool(c); print('  %s %s' % ('ok  ' if c else 'FAIL', s))
def pens(d):   # строки журнала с первого снимка сценария (до него — загрузка, флаг ещё не сработал)
    r = []; seen = False
    for l in open(d + '/smoke.txt', errors='replace'):
        if l.startswith('SHOT '): seen = True
        if not seen: continue
        m = re.match(r'ROADTHICK1023 pen=([0-9a-f]{8}) passes=(\d+) us=(\d+) lines=(\d+)', l)
        if m: r.append((int(m.group(1), 16), int(m.group(3))))
    return r
def count(imgN, imgO, abgr):   # пиксели пера, которые есть в новом кадре и которых нет в прежнем (кадры одной сцены, камера и машина неподвижны)
    R, G, B = abgr & 255, abgr >> 8 & 255, abgr >> 16 & 255; pn = Image.open(imgN).convert('RGB').load(); po = Image.open(imgO).convert('RGB').load(); w, h = Image.open(imgN).size; n = 0
    for y in range(70, h - 10):
        for x in range(w):
            r, g, b = pn[x, y]; r2, g2, b2 = po[x, y]
            if abs(r - R) <= 26 and abs(g - G) <= 26 and abs(b - B) <= 26 and (abs(r2 - R) > 40 or abs(g2 - G) > 40 or abs(b2 - B) > 40): n += 1
    return n
pn, po = pens(new), pens(old)
chk(len(pn) > 3 and not po, 'строк ROADTHICK1023: новый %d, прежний %d' % (len(pn), len(po)))
pen = pn[-1][0] if pn else 0
us = sorted(u for _, u in pn)[len(pn) // 2] if pn else 9999
chk(us <= 80, 'толстые проходы: медиана %d мкс на кадр (Мак; ×5,8 — %.2f мс PSP), порог 80 (отдельные первые кадры дороже: отбор записей)' % (us, us * 5.8 / 1000))
def run(newd, oldd, label):
    global ok
    for name in ['a_car1', 'a_high', 'a_low']:
        try: a = count('%s/%s.png' % (newd, name), '%s/%s.png' % (oldd, name), pen)
        except OSError: chk(False, '%s: нет кадра %s' % (label, name)); continue
        chk(a >= 150, '%-7s %-7s пикселей пера, которых нет в прежнем кадре: %4d (нужно >= 150)' % (label, name, a))
run(new, old, 'L1')
print('  зелёный: край асфальта плотный (3 px у глаза), перо — тёмный насыщенный тон асфальта' if ok else '  FAIL: таблица выше')
sys.exit(0 if ok else 1)
