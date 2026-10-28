#!/usr/bin/env python3
# №1028: check.py ПАПКА_НОВОГО — своя машина L1: на 8 ракурсах вокруг неё (az150/210/120/240 × 4,5 и 6,5 м) тёмных точек обводки в кадре у «нового» (flag carink1028 1) меньше, чем у «старого» (0, как TEST71),
# не меньше чем на 2,5 %, но не меньше 70 % прежнего (контур вокруг силуэта остаётся); виды сверху/сбоку/с торца изменились, посадка и выход дают кадры. Последняя строка PASS/FAIL
import sys, os
import numpy as np
from PIL import Image
d = sys.argv[1]; old = sys.argv[2] if len(sys.argv) > 2 else d   # второй аргумент — папка «старого» (для красного контроля: тот же)
names = open(os.path.join(os.path.dirname(__file__), 'names.txt')).read().split()
def im(dd, n):
    p = f'{dd}/{n}.bmp'; return np.asarray(Image.open(p).convert('RGB'), float) if os.path.exists(p) else None
ink = lambda X: int(((X[60:262] < 45).all(2)).sum())
ok = True; n_ok = 0
for n in names:
    A, B = im(d, f'{n}_new'), im(old, f'{n}_old')
    if A is None or B is None: print(n, 'нет снимка'); ok = False; continue
    a, b = ink(A), ink(B); r = (b - a) / max(b, 1); good = r >= .025 and a >= .7 * b; n_ok += good
    print(f'{n}: тёмных точек {b} -> {a} ({r:+.1%})', 'ok' if good else 'ПАДАЕТ'); ok &= good
for t in ('rs', 'side', 'top', 'end'):
    A, B = im(d, f'{t}_new'), im(old, f'{t}_old')
    if A is None or B is None: print(t, 'нет снимка'); ok = False; continue
    ch = int((np.abs(A - B).max(2) > 25).sum()); print(f'{t}: изменено точек {ch}', 'ok' if ch >= 150 else 'ПАДАЕТ'); ok &= ch >= 150
for t in ('foot_new', 'board_a', 'board_b', 'board_c', 'incar_new'):
    good = im(d, t) is not None; print(t, 'есть' if good else 'НЕТ'); ok &= good
print('PASS' if ok else 'FAIL')
