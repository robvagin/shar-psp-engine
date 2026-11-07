#!/usr/bin/env python3
"""№1170: после конца камеры старта миссии глаз игровой камеры не подходит к герою ближе MIN_OK м (прежнее: переход идёт по
прямой от позы конца облёта к позе игровой камеры и проходит у самой головы героя).
Вход: каталоги прогонов smoke (с `flag camlog`, `flag startcam 1`); по каждому — строки MSNCAM end, CAMF.
Выход: на каждый прогон минимум расстояния «глаз — голова героя» за 1,5 с после конца облёта; код 1, если меньше MIN_OK."""
import re, sys, math
MIN_OK = 2.2   # м: ближе голова героя занимает кадр целиком (у игровой камеры пешком 4,2-5 м)
def run(d):
    end = None; best = (9e9, 0); first = None
    for l in open(d + "/smoke.txt", errors="replace"):
        m = re.match(r"MSNCAM end t=([\d.]+)", l)
        if m and end is None: end = float(m.group(1)); continue
        m = re.match(r"CAMF t=([\d.]+) st=\d+ fov=[\d.]+ eye=([-\d.]+),([-\d.]+),([-\d.]+) look=\S+ homer=([-\d.]+),([-\d.]+),([-\d.]+)", l)
        if m and end is not None:
            t, ex, ey, ez, hx, hy, hz = map(float, m.groups())
            if t > end + 1.5: break
            dd = math.dist((ex, ey, ez), (hx, hy + 1.5, hz))
            if first is None: first = dd
            if dd < best[0]: best = (dd, t - end)
    return end, first, best
bad = 0
for d in sys.argv[1:]:
    end, first, best = run(d)
    if end is None: print(d, "камера старта не играла"); bad += 1; continue
    ok = best[0] >= MIN_OK; bad += not ok
    print(f"{d.split('/')[-1]}: конец облёта {end:.1f} с, первый кадр {first:.1f} м, ближе всего {best[0]:.2f} м через {best[1]:.2f} с  {'OK' if ok else 'БЛИЗКО'}")
sys.exit(1 if bad else 0)
