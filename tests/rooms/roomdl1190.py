#!/usr/bin/env python3
"""ТЕСТ90 (листы комнат): разбор прогона tests/rooms/roomdl1190.txt (build/smoke/<время>).
 1) в каждой точке (игра на паузе: Гомер и прохожие стоят) по три кадра с листами (a1..a3), по три прямым путём (b1..b3) и снова с листами (c1..c3): пиксель «неподвижный», если он один и тот же во всех кадрах каждой тройки и в a1 с c1 (комната); Гомер, прохожие, значок △
    живут по времени и из счёта выпадают; на неподвижных пикселях вне доски паузы кадр с листами обязан совпасть с прямым побайтно;
 2) в журнале ROOMDL ev=build для каждой комнаты (группы, куски, байты листов, мс сборки), ни одной ROOMDL ev=fail;
 3) цена (второй прогон, tests/rooms/roomdl1190_cost.txt): рабочее время кадра сцен RDLa_* против RDLb_* (колонки отчёта summary.txt) — листы не хуже прямого пути; сводка в конце.
  python3 tests/rooms/roomdl1190.py build/smoke/<время картинки> [build/smoke/<время цены>]      код 1 — провал (красный контроль: flag roomdlbug 1 в начале сценария)"""
import re, sys
from pathlib import Path
from PIL import Image, ImageChops, ImageFilter
d = Path(sys.argv[1]); bad = 0
import numpy as np
OVER = np.zeros((272, 480), bool); OVER[10:260, 118:366] = True; OVER[0:44, 396:480] = True; OVER[246:272, 0:80] = True   # px PSP
names = sorted({m.group(1) for f in d.glob('a1_*.png') for m in [re.match(r'a1_(.+)\.png', f.name)]})
for nm in names:
    A = [np.array(Image.open(d / ('a%d_%s.png' % (k, nm))).convert('RGB')).astype(int) for k in (1, 2, 3)]
    B = [np.array(Image.open(d / ('b%d_%s.png' % (k, nm))).convert('RGB')).astype(int) for k in (1, 2, 3)]
    C = [np.array(Image.open(d / ('c%d_%s.png' % (k, nm))).convert('RGB')).astype(int) for k in (1, 2, 3)]
    st = np.ones(A[0].shape[:2], bool)
    for X in (A, B, C):
        for k in (1, 2): st &= (X[0] == X[k]).all(axis=2)   # неподвижен внутри каждой тройки
    st &= (A[0] == C[0]).all(axis=2)   # и между кадрами листов до и после прямого пути (медленное движение: уборщица, лампы — не в счёт)
    diff = (A[0] != B[0]).any(axis=2) & st; diff &= ~OVER   # доска паузы, счёт монет и подпись угла: у них своя анимация (блик на CONTINUE), комнату они не рисуют
    n = int(diff.sum()); ns = int(st.sum()); big = int((diff & (np.abs(A[0] - B[0]).max(axis=2) > 16)).sum()); bad |= n != 0
    print('%s %-28s неподвижных пикселей %6d из %d, различий листы/прямой вне доски паузы: %d (из них больше 16 по каналу: %d)' % ('ok  ' if n == 0 else 'FAIL', nm, ns, st.size, n, big))
txt = (d / 'smoke.txt').read_bytes().decode('latin-1')
builds = re.findall(r'ROOMDL ev=build room=(\d+) groups=(\d+) pieces=(\d+) bytes=(\d+) heap=(\d+) ms=([\d.]+)', txt); fails = txt.count('ROOMDL ev=fail')
print('сборок листов %d, отказов %d' % (len(builds), fails)); bad |= (not builds) or fails > 0
for b in builds: print('  комната %s: групп %s, кусков %s, листы %.1f КБ (два экземпляра), сборка %s мс' % (b[0], b[1], b[2], int(b[3]) / 1024., b[5]))
cd = Path(sys.argv[2]) if len(sys.argv) > 2 else d   # прогон цены (tests/rooms/roomdl1190_cost.txt) — отдельный: кадры картинки сняты в паузе
summ = (cd / 'summary.txt').read_text(encoding='utf-8', errors='replace').splitlines()
rows = {}
for ln in summ:
    p = ln.split()
    if len(p) > 6 and p[0].startswith('RDL') and p[1] in ('long', 'view60'):
        rows[p[0]] = p
sa = sb = n_ = 0
for nm in names:
    ra, rb = rows.get('RDLa_' + nm), rows.get('RDLb_' + nm)
    if ra and rb:
        wa, wb = float(ra[5]), float(rb[5]); sa += wa; sb += wb; n_ += 1
        print('  %-26s work p95 листы %.2f мс, прямой %.2f мс' % (nm, wa, wb))
if n_: print('среднее work p95 по %d точкам: листы %.2f, прямой %.2f мс (эмулятор, не железо)' % (n_, sa / n_, sb / n_))
print('FAIL' if bad else 'PASS'); sys.exit(bad)
