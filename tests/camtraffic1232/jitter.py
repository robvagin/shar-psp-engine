#!/usr/bin/env python3
# №1232: дрожь камеры рядом с машиной трафика. jitter.py SMOKE [сцена] — по строкам CAMTRF (глаз, ближайшая машина): вторая разность глаза (м) по кадрам, по корзинам dmin.
# Дрожь = |e(t+1) − 2e(t) + e(t−1)|, только по соседним кадрам (шаг < 0,06 с), Гомер стоит. Печатает среднее и 95-й процентиль по корзинам и число смен знака скорости глаза за секунду.
import re, sys, math
f = sys.argv[1]; scene = sys.argv[2] if len(sys.argv) > 2 else None
txt = open(f, errors='ignore').read()
t0 = 0.0
if scene:
    m = re.search(r'SCENE %s (\S+)' % scene, txt); t0 = float(m[1]) if m else 0.0
rows = [(float(m[1]), (float(m[2]), float(m[3]), float(m[4])), float(m[10]), float(m[11])) for m in re.finditer(r'CAMTRF t=(\S+) eye=(\S+),(\S+),(\S+) look=(\S+),(\S+),(\S+) home=(\S+),(\S+) dmin=(\S+) v=(\S+) car=(\d+)', txt) if float(m[1]) >= t0]
b = {'<3 м': [], '3–5 м': [], '5–8 м': [], '8–14 м': []}
for i in range(1, len(rows) - 1):
    a, c, d = rows[i - 1], rows[i], rows[i + 1]
    if not (0 < c[0] - a[0] < .06 and 0 < d[0] - c[0] < .06): continue
    if c[3] < 2.0: continue   # машина стоит — не «проезжающая»
    j = math.sqrt(sum((d[1][k] - 2 * c[1][k] + a[1][k]) ** 2 for k in range(3)))
    key = '<3 м' if c[2] < 3 else '3–5 м' if c[2] < 5 else '5–8 м' if c[2] < 8 else '8–14 м'
    b[key].append(j)
for k, v in b.items():
    v.sort(); n = len(v)
    print('%-7s кадров %4d  дрожь среднее %.4f м  p95 %.4f м  макс %.4f м' % (k, n, sum(v) / n if n else 0, v[int(.95 * (n - 1))] if n else 0, v[-1] if n else 0))
