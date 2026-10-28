#!/usr/bin/env python3
"""№1063 accept60: свечение коллекционной карточки — векторное пятно оригинала. python3 check.py НОВЫЙ СТАРЫЙ [--expect fail]  (каталоги out_n прогонов look.txt и look_old.txt: flag card1063old 1).
Данные оригинала (CARDGLOW в журнале): один билборд-квад 1,6 м со сложением, цвет вершины белый, текстура — серый радиальный градиент. Проверки:
 (1) журнал нового: векторное пятно нарисовано (CARDSPOT ≥ 3), полуширина и полувысота 0,80 ±0,02 м (= 1,6 м данных); у старого пятна нет;
 (2) старого блика-звезды у карточки нет (GQ944 site=coll в 0,6 м от карточки): у нового 0, у старого ≥ 1; старый размытый спрайт (текстурный билборд) у карточки не рисуется ни в одном;
 (3) кадры с 10, 5 и 2 м (камера cam): разница нового и старого есть (≥ 20 точек > 6) и её ширина = диаметр пятна в кадре (1,6 м при fovy 48,8°) в пределах 0,75…1,1; нигде больше разницы нет
Красный: «новый» = прогон с flag card1063old 1: пятна и разницы нет"""
import math, re, sys
from PIL import Image
import numpy as np
a = [x for x in sys.argv[1:] if not x.startswith('--')]; exp_fail = '--expect' in sys.argv and sys.argv[sys.argv.index('--expect') + 1] == 'fail'
N, O = a[0], a[1]; ok = True
def chk(name, cond, msg):
    global ok; print(('  ok  ' if cond else '  FAIL ') + name + ': ' + msg); ok = ok and cond
CARD = (408.0, 4.9, -413.1)
def log(p):
    s = open(p, errors='replace').read()
    sp = [(float(m.group(1)), float(m.group(2))) for m in re.finditer(r'^CARDSPOT n=\d+ a=\S+ c=408\.0,\S+ half=(\S+),(\S+)', s, re.M)]
    gq = 0
    for m in re.finditer(r'^GQ944 lv=1 site=coll p=(\S+),(\S+),(\S+) ', s, re.M):
        x, y, z = map(float, m.groups())
        if math.dist((x, y, z), CARD) < .6: gq += 1
    bill = len(re.findall(r'^GQ944 .*site=coll_bill', s, re.M))
    return sp, gq, bill
spn, gqn, bn = log(f'{N}/smoke.txt'); spo, gqo, bo = log(f'{O}/smoke.txt')
chk('пятно в журнале', len(spn) >= 3 and all(abs(h - .8) <= .02 and abs(u - .8) <= .02 for h, u in spn) and not spo, f'CARDSPOT у нового {len(spn)} (полуширина {spn[0][0] if spn else "-"} м, цель 0,80 = 1,6 м данных), у старого {len(spo)}')
chk('блик-звезда', gqn == 0 and gqo >= 1, f'GQ944 у карточки: новый {gqn} (нужно 0), старый {gqo} (≥ 1)')
chk('старого спрайта нет', bn == 0 and bo == 0, f'текстурный билборд у карточки: новый {bn}, старый {bo}')
for d in (10, 5, 2):
    A = np.asarray(Image.open(f'{O}/d{d}_0.png').convert('RGB')).astype(int); B = np.asarray(Image.open(f'{N}/d{d}_0.png').convert('RGB')).astype(int)
    df = np.abs(A - B).max(axis=2) > 6; df[:64] = False; ys, xs = np.nonzero(df); want = 1.6 / d * 136 / math.tan(math.radians(24.4))
    wid = (xs.max() - xs.min() + 1) if len(xs) else 0
    chk(f'кадр {d} м', df.sum() >= 20 and .75 * want <= wid <= 1.1 * want, f'разница {int(df.sum())} точек, ширина {wid} px, диаметр пятна 1,6 м в кадре {want:.0f} px (нужно {0.75 * want:.0f}…{1.1 * want:.0f})')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok != exp_fail else 1)
