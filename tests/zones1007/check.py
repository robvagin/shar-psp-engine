#!/usr/bin/env python3
# №1007б: check.py НОВЫЙ СТАРЫЙ — сайдкар zones1007p.bin подгружен на уровнях с площадками (строки ZONES1007P), у «старого» (файл zones1007old) выключен; на видах сверху площадка заметно другого тона
# (≥ 400 точек с разницей > 25 из 255 и сдвиг среднего цвета изменённых точек ≥ 12), остальной кадр в пределах (≤ 15 % точек), последняя строка PASS/FAIL
import sys, re, os
import numpy as np
from PIL import Image
new, old = sys.argv[1], sys.argv[2]
ok = True
def zp(p):
    return {int(m[1]): (int(m[2]), int(m[3])) for l in open(p + '/smoke.txt', errors='ignore') if (m := re.match(r'ZONES1007P level=(\d+) pieces=(\d+) on=(\d)', l))}
zn, zo = zp(new), zp(old)
for lv in (1, 2, 4, 5, 7):
    a, b = zn.get(lv), zo.get(lv)
    good = a and a[0] > 0 and a[1] == 1 and b and b[0] > 0 and b[1] == 0
    print(f'L{lv}: кусков в сайдкаре {a[0] if a else "-"}, включён у нового {a[1] if a else "-"}, у старого {b[1] if b else "-"}', 'ok' if good else 'ПАДАЕТ'); ok &= bool(good)
def img(d, n):
    p = f'{d}/{n}.bmp'
    return np.asarray(Image.open(p).convert('RGB'), float) if os.path.exists(p) else None
for tag in ('L1', 'L2', 'L4', 'L5', 'L7'):
    A, B = img(new, f'{tag}_top'), img(old, f'{tag}_top')
    if A is None or B is None: print(tag, 'нет снимка'); ok = False; continue
    d = np.abs(A - B).max(2); m = d > 25; n = int(m.sum()); frac = n / d.size
    sh = float(np.abs(A[m].mean(0) - B[m].mean(0)).max()) if n else 0.0
    good = n >= 400 and sh >= 12 and frac <= .15
    print(f'{tag} top: изменённых точек {n} ({frac:.1%}), сдвиг среднего цвета {sh:.1f}', 'ok' if good else 'ПАДАЕТ'); ok &= good
print('PASS' if ok else 'FAIL')
