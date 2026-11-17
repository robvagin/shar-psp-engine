#!/usr/bin/env python3
import os, sys
import numpy as np
from PIL import Image
out = sys.argv[1]
def frames(v):
    return [np.asarray(Image.open(os.path.join(out, 'out_lm1360_%d' % v, 'm%d.bmp' % i)).convert('RGB'))[:272, :480].astype(int) for i in range(14)]
def yel(im): r, g, b = im[..., 0], im[..., 1], im[..., 2]; return (r > 190) & (g > 160) & (b < 120)
def bbox(m, x0, y0):
    ys, xs = np.where(m); return (xs.min() + x0, xs.max() + x0, ys.min() + y0, ys.max() + y0) if len(xs) else None
ok = True
def chk(cond, why):
    global ok; ok &= bool(cond); print('  %-104s %s' % (why, 'ok' if cond else 'FAIL'))
new, old = frames(0), frames(1)
circ, text = [], []
for im in new:
    m = yel(im)[190:272, 350:480]
    c = bbox(m[:, 70:], 350 + 70, 190); t = bbox(m[:, :68], 350, 190)   # кружок: x >= 420; надпись: левее
    circ.append(c); text.append(t)
vis = [i for i, c in enumerate(circ) if c and (c[1] - c[0]) >= 18]
print('кадров со знаком: %d из %d' % (len(vis), len(new)))
chk(len(vis) >= 6, 'новое: знак виден на закрытом экране (кадров %d)' % len(vis))
if vis:
    cx = [(circ[i][0] + circ[i][1]) / 2 for i in vis]; cy = [(circ[i][2] + circ[i][3]) / 2 for i in vis]; dia = [max(circ[i][1] - circ[i][0], circ[i][3] - circ[i][2]) + 1 for i in vis]
    ex, ey = 480 - 22 - 16, 272 - 18 - 16
    print('центр знака x %.1f..%.1f y %.1f..%.1f (ожидание %d, %d); диаметр %d..%d' % (min(cx), max(cx), min(cy), max(cy), ex, ey, min(dia), max(dia)))
    chk(all(abs(x - ex) <= 1.5 for x in cx) and all(abs(y - ey) <= 1.5 for y in cy), 'новое: центр знака в углу с полями радара (22 справа, 18 снизу) ±1 px')
    chk(max(circ[i][1] for i in vis) <= 480 - 22 + 1, 'новое: правый край знака не дальше поля 22 px')
    chk(min(dia) <= .9 * max(dia), 'новое: размер пульсирует (меньший %d px, больший %d px)' % (min(dia), max(dia)))
    ty = [(text[i][2] + text[i][3]) / 2 for i in vis if text[i]]
    chk(ty and all(abs(t - cy[k]) <= 1.5 for k, t in enumerate(ty[:len(cy)])), 'новое: надпись LOADING по центру кружка по высоте (разница %.1f px)' % (max(abs(t - c) for t, c in zip(ty, cy)) if ty else -1))
oldvis = [1 for im in old if (lambda b: b and (b[1] - b[0]) >= 18)(bbox(yel(im)[190:272, 420:480], 420, 190))]
chk(len(oldvis) == 0, 'красный контроль: на старом знака нет (кадров %d)' % len(oldvis))
print('LOADMARK1360', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
