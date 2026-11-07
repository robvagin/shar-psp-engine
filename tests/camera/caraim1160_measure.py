#!/usr/bin/env python3
"""№1160: где машина по высоте кадра на ступенях Car Aim. Запуск: python3 tests/camera/caraim1160_measure.py <папка прогона> [<папка прогона OLD для побитового сравнения ступени 2>]
Машина — розовый кузов Honest Abe (связная область по цвету от семени у центра низа кадра); строка: ступень, кадр, верх и низ кузова в пикселях (кадр 480×272), центр, зазор до нижнего края.
Побитовое сравнение: снимки aim2_* нового прогона против снимков OLD (CARAIM1160_OLD=1) — все пиксели одинаковы."""
import sys, os
from PIL import Image
import numpy as np
def _dil(m):
    o = m.copy(); o[1:] |= m[:-1]; o[:-1] |= m[1:]; o[:, 1:] |= m[:, :-1]; o[:, :-1] |= m[:, 1:]; return o
def car_bbox(im):
    a = np.asarray(im.convert('RGB')).astype(int); R, G, B = a[..., 0], a[..., 1], a[..., 2]
    m = (R > 150) & (G < 130) & (B > 80) & (B < 215) & (R - G > 80)
    m[:, :150] = False; m[:, 330:] = False; m[:60, :] = False   # колонки машины, не небо
    m = _dil(_dil(m))   # склеить блики и швы кузова
    h, w = m.shape; seen = np.zeros_like(m); best = None; ba = 0
    for y in range(h):
        for x in range(w):
            if m[y, x] and not seen[y, x]:
                st = [(y, x)]; seen[y, x] = True; pts = []
                while st:
                    cy, cx = st.pop(); pts.append((cy, cx))
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not seen[ny, nx]: seen[ny, nx] = True; st.append((ny, nx))
                ys = [p[0] for p in pts]; xs = [p[1] for p in pts]
                if len(pts) > ba and min(xs) < 300 and max(xs) > 180: ba = len(pts); best = (min(ys), max(ys), min(xs), max(xs))   # самая крупная розовая область, перекрывающая середину кадра
    return best
def main():
    d = sys.argv[1]; res = {}
    for st in (2, 0, 4):
        for k in ('still', 'straight', 'turn', 'skid'):
            p = os.path.join(d, 'aim%d_%s.png' % (st, k))
            if not os.path.exists(p): continue
            b = car_bbox(Image.open(p)); res[(st, k)] = b
            print('ступень %d  %-9s ' % (st, k) + ('нет кузова в кадре' if b is None else 'верх %3d  низ %3d  высота %3d  центр %5.1f  до нижнего края %3d px' % (b[0], b[1], b[1] - b[0], (b[0] + b[1]) / 2, 271 - b[1])))
    if len(sys.argv) > 2:
        o = sys.argv[2]; same = True
        for k in ('still', 'straight', 'turn', 'skid'):
            a, b = os.path.join(d, 'aim2_%s.png' % k), os.path.join(o, 'aim2_%s.png' % k)
            if os.path.exists(a) and os.path.exists(b):
                eq = np.array_equal(np.asarray(Image.open(a).convert('RGB')), np.asarray(Image.open(b).convert('RGB'))); same &= eq
                print('ступень 2 %-9s против OLD: %s' % (k, 'побитово одинаково' if eq else 'ОТЛИЧАЕТСЯ'))
        print('ПОБИТОВО' if same else 'РАЗНИЦА')
main()
