#!/usr/bin/env python3
# №1034: скамейка твёрдая для Гомера пешком и держит сверху. check.py new|old SMOKE БЕНЧ_X БЕНЧ_Y_ЦЕНТРА   (строки BENCH1034 flag benchlog 1; сцены B_old_N / B_new_N из bench_L*.txt)
#   new: в фазе «вперёд 2,5 с» Гомер упирается (x не меньше центр + 0,45 м: подход с +x), при прыжке на упоре поднимается не ниже верха скамейки (y >= земля + 0,6) на её x-интервале
#   block: «новое» упирается (баки, ящики: без прыжка), x не меньше центр + 0,45
#   old: красный контроль — в той же фазе проходит насквозь (x <= центр − 1,0)
import re, sys
mode, f, cx, cy = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]); cz = float(sys.argv[5]) if len(sys.argv) > 5 else None   # cy — высота центра скамейки (верх = cy + 0,41)
txt = open(f, errors='ignore').read()
rows = [(float(m[1]), float(m[2]), float(m[3]), int(m[5]), float(m[4])) for m in re.finditer(r'BENCH1034 t=(\S+) x=(\S+) y=(\S+) z=(\S+) g=(-?\d+)', txt)]   # t, x, y, grounded, z
sc = {m[1]: float(m[2]) for m in re.finditer(r'SCENE (B_(?:old|new)_\d) (\S+)', txt)}
key = [k for k in sc if k.startswith('B_' + ('new' if mode == 'block' else mode))]
if not key: print('нет сцены', mode); sys.exit(2)
t0 = sc[key[0]]
ph = lambda a, b: [r for r in rows if t0 + a <= r[0] < t0 + b]
walk, jump, start = ph(2.5, 6.0), ph(6.0, 11.8), ph(0.5, 2.4)
if mode == 'block': jump = [(0, 0, 0, 0, 0)]   # без прыжка: бак выше скамейки
if not walk or not jump or not start: print('мало строк'); sys.exit(2)
gy = start[-1][2]; sgn = 1 if start[-1][1] > cx else -1
xs = [r[1] for r in walk]; depth = (min(xs) - cx) * sgn if sgn > 0 else (cx - max(xs)) * sgn * -1
reach = min((x - cx) * sgn for x in xs)          # насколько близко к центру подошёл; < 0 — прошёл насквозь
top = max((r[2] for r in jump if abs(r[1] - cx) <= .6 and r[3]), default=gy)   # стоит на опоре (grounded) над скамейкой
print('%s: стоит у скамейки на %.2f м от центра (проход насквозь, если < 0), стоял сверху на y %.2f (земля %.2f, верх скамейки %.2f)' % (mode, reach, top, gy, cy + .41))
if mode == 'block':   # твёрдый: упирается, либо обходит сбоку (скользит вдоль коробки): в момент пересечения центра по x уходит от оси не меньше чем на 1,0 м
    cross = [r for r in walk if (r[1] - cx) * sgn <= 0]
    dev = abs(cross[0][4] - cz) if cross and cz is not None else 0.0
    print('block: упёрся на %.2f м от центра, обход сбоку при пересечении центра: %.2f м от оси' % (reach, dev)); sys.exit(0 if reach >= .45 or dev >= 1.0 else 1)
if mode == 'new': sys.exit(0 if reach >= .45 and abs(top - (cy + .41)) <= .15 else 1)
sys.exit(0 if reach <= -1.0 else 1)
