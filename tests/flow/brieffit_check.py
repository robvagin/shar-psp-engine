#!/usr/bin/env python3
"""Карточка брифинга: подписи кнопок (△ Cancel Mission, ✕ Continue) внутри рамки с отступом. Читает строки BRIEFFIT smoke.txt каталога прогона.
   Зелёный: у каждой группы margin_l >= 8 и margin_r >= 8 (строки есть). Красный (EBOOT с -DBRIEFFIT_OLD=1): должен упасть по margin_r < 8."""
import sys, re, glob, os
d = sys.argv[1]; MIN = 8
txt = open(os.path.join(d, 'smoke.txt'), 'rb').read().decode('utf-8', 'replace')
rows = re.findall(r'BRIEFFIT tag=(\d+) x0=(-?\d+) x1=(-?\d+) card=(-?\d+)\.\.(-?\d+) dx=(-?\d+) margin_l=(-?\d+) margin_r=(-?\d+)', txt)
print('групп кнопок в журнале:', len(rows))
bad = 0
for t, x0, x1, cl, cr, dx, ml, mr in rows:
    ok = int(ml) >= MIN and int(mr) >= MIN
    print(' метка %s: текст %s..%s, карточка %s..%s, сдвиг %s, отступ слева %s справа %s %s' % (t, x0, x1, cl, cr, dx, ml, mr, 'OK' if ok else 'FAIL'))
    bad += not ok
if not rows: print('FAIL: строк BRIEFFIT нет (карточка не показана)'); sys.exit(1)
sys.exit(1 if bad else 0)
