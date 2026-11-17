#!/usr/bin/env python3
"""1350: на паузе миссии рамка задания: (1) по центру экрана: центр рамки по X = 240 ±2 (из строки PAUSEOBJ1350 — самой рамки, без значка, выступающего слева);
(2') значок прилеплен к рамке: между значком и рамкой нет разрыва шире 4 px (на снимке колонки, где есть отличия от эталона); (2) не заходит на рамку меню: на снимке в прямоугольнике рамки меню (x 118..364, y 170..197: нижняя полоса рамки меню; выше — пульс строки CONTINUE даёт шум) пикселей, отличных от эталона паузы без окна задания, 0;
на старом (PAUSEOBJPOS1350_OLD=1, длинный текст) такие пиксели есть; (3) верх группы рамки не выше низа меню + 6 px; (4) размер рамки как у HUD: ширина/высота равны старым
(строка PAUSEOBJ1350 только у нового, поэтому высоту сверяем по снимку: высота группы из лога = высоте области отличий старого варианта ±3 px). Использование: check.py ПАПКА КОД..."""
import os, re, sys
import numpy as np
from PIL import Image
out = sys.argv[1]; codes = sys.argv[2:] or ['302', '303']
def img(c, v): return np.asarray(Image.open(os.path.join(out, 'out_po1347_%s_%s' % (c, v), 'p.bmp')).convert('RGB'))[:272, :480].astype(int)
def log(c, v): return open(os.path.join(out, 'out_po1347_%s_%s' % (c, v), 'smoke.txt'), errors='replace').read()
FR = (118, 170, 364, 197)
ok = True
def chk(cond, why):
    global ok; ok &= bool(cond); print('  %-92s %s' % (why, 'ok' if cond else 'FAIL'))
for c in codes:
    ref, new, old = img(c, 2), img(c, 0), img(c, 1)
    def diff(a, b, box=None):
        d = np.abs(a - b).max(2) > 40
        if box: x0, y0, x1, y1 = box; d = d[y0:y1, x0:x1]
        return d
    inter_new = int(diff(new, ref, FR).sum()); inter_old = int(diff(old, ref, FR).sum())
    m = re.findall(r'^PAUSEOBJ1350 frame x0=([\d.]+) x1=([\d.]+) cx=([\d.]+) group y0=([\d.]+) y1=([\d.]+) menu_bot=([\d.]+)', log(c, 0), re.M)
    print('код %s: пересечение с рамкой меню: новое %d px, старое %d px; лог %s' % (c, inter_new, inter_old, m[-1] if m else None))
    chk(inter_new == 0, 'новое: пересечение рамки задания с рамкой меню 0 px')
    chk(m, 'новое: строка PAUSEOBJ1350 есть')
    if m:
        x0, x1, cx, y0, y1, mb = map(float, m[-1])
        chk(abs(cx - 240) <= 2, 'новое: центр группы «значок + рамка» по X = %.1f (240 ±2)' % cx)
        chk(y0 >= mb + 6 or y1 >= 267, 'новое: верх рамки %.1f ниже низа меню %.0f с зазором (или рамка у низа экрана %.1f)' % (y0, mb, y1))
        chk(y0 >= mb + 1, 'новое: верх рамки не выше низа меню')
        def cap_h(d):   # высота рамки без значка: колонки правее значка (значок слева выступает за рамку и у старого срезан краем экрана)
            xs = np.where(d.any(0))[0]; cols = d[:, xs.min() + 70:xs.max() - 10]; ys = np.where(cols.any(1))[0]
            return (ys.max() - ys.min() + 1) if len(ys) else 0
        dn = diff(new, ref); dn[:190] = False; do = diff(old, ref); do[:190] = False
        def gap(d):
            xs = np.where(d.any(0))[0]; g = 0; best = 0
            for x in range(xs.min(), xs.max() + 1):
                g = 0 if d[:, x].any() else g + 1; best = max(best, g)
            return best
        chk(gap(dn) <= 4, 'значок прилеплен к рамке: разрыв по горизонтали %d px (старое %d px)' % (gap(dn), gap(do)))
        hn, ho = cap_h(dn), cap_h(do)
        chk(hn >= 40, 'новое: рамка нарисована (высота %d px)' % hn)
        chk(abs(hn - ho) <= 2, 'размер не изменён: высота рамки новое %d px, старое %d px' % (hn, ho))
    if c == codes[-1] or c == '303': chk(inter_old > 0, 'красный контроль: на старом пересечение есть (%d px)' % inter_old)
print('PAUSEOBJ1350', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
