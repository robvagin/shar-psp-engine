#!/usr/bin/env python3
"""1347: на снимке паузы миссии внизу слева (x 15..470, y 200..250 (с 1350 рамка по центру, до 1350 внизу слева)) должна быть рамка задания с текстом этапа. Признак: жёлтые пиксели текста (R,G > 170, B < 90) в окне
рамки — на HUD и в паузе подпись задания жёлтая; без окна в этой области мир/трава без жёлтого. Новое: >= MIN; старое (PAUSEOBJ1347_OLD=1): ~0 (красный контроль).
Второй признак: после выхода из паузы (r) HUD такой же, как до неё (a): расхождение по значку в левом верхнем углу <= 4 %. Использование: check.py ПАПКА КОД..."""
import sys, os
import numpy as np
from PIL import Image
out = sys.argv[1]; codes = sys.argv[2:] or ['101', '302']
def load(n, f):
    p = os.path.join(out, 'out_po1347_%s_%s' % (n, f) , '%s.bmp' % f) if False else os.path.join(out, 'out_po1347_%s' % n, '%s.bmp' % f)
    return np.asarray(Image.open(p).convert('RGB'))[:272, :480].astype(int)
def yellow(im):
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    return int(((r > 170) & (g > 170) & (b < 90))[200:250, 15:470].sum())
ok = True; MIN = 25
for c in codes:
    new = yellow(load('%s_0' % c, 'p')); old = yellow(load('%s_1' % c, 'p'))
    a = load('%s_0' % c, 'a'); r = load('%s_0' % c, 'r')
    corner = np.abs(a[0:60, 0:80] - r[0:60, 0:80]).sum(2) > 40; diff = corner.mean()
    good = new >= MIN and old < MIN / 2 and diff < .04
    print('code %s: жёлтых пикселей рамки на паузе новое %d, старое %d; значок в углу после паузы расхождение %.1f %% -> %s' % (c, new, old, diff * 100, 'PASS' if good else 'FAIL'))
    ok &= good
print('PAUSEOBJ1347', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
