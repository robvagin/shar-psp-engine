#!/usr/bin/env python3
"""№1160: закон объектива на сдвинутом кадре. Прогон tests/camera/camin_car.txt с «flag camuseraim N» (смоук, строки CAMIN движка): на каждую сцену — последняя строка CAMIN:
кадров, «внутри» (any), глаз / ближняя плоскость / низ в геометрии. Зелёный: в каждой сцене кадров ≥ 60 и any = 0 при ступени 4 так же, как при ступени 2. Запуск: caraim1160_camin.py <папка прогона> [<папка прогона ступени 2>]"""
import re, sys
def scenes(d):
    out = []; cur = None
    for l in open(d + '/smoke.txt', errors='replace'):
        if l.startswith('SCENE '): cur = l.split()[1]
        m = re.match(r'CAMIN t=\S+ frames=(\d+) any=(\d+) eye=(\d+) near=(\d+) ground=(\d+) car=(\d+) person=(\d+)', l)
        if m: out.append((cur,) + tuple(int(x) for x in m.groups()))
    return out
a = scenes(sys.argv[1]); b = scenes(sys.argv[2]) if len(sys.argv) > 2 else None
print('сцена             кадров  внутри  глаз  плоскость  низ  машина  человек' + ('   (ступень 2: кадров / внутри)' if b else ''))
bad = 0
for i, r in enumerate(a):
    extra = ''
    if b and i < len(b): extra = '   %d / %d' % (b[i][1], b[i][2])
    flag = '' if r[2] == 0 else '  <- ВНУТРИ'
    bad += r[2] != 0
    print('%-16s %6d %7d %5d %9d %4d %7d %8d%s%s' % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], extra, flag))
print('сцен с кадрами «внутри»: %d из %d' % (bad, len(a))); sys.exit(1 if bad else 0)
