#!/usr/bin/env python3
"""Аудит камер старта: по каталогам прогонов smoke (с `flag camlog`) на КАЖДОЕ окончание камеры старта (MSNCAM end) — фаза, длина,
расстояние «глаз — голова героя» в последнем кадре облёта и ближайший подход за 1,5 с после конца (выход в игровую камеру).
Код 1, если где-то ближе MIN_OK м (голова героя заполняет кадр).   python3 tests/camera/startcam_report.py КАТАЛОГ_ПРОГОНА [...]"""
import re, sys, math
MIN_OK = 2.2
def run(d):
    ev, cur = [], None; phase = {}; last_eye = None; ph = None
    for l in open(d + "/smoke.txt", errors="replace"):
        m = re.match(r"MSNCAM start \S+ \S+ phase=(\S+) frames=(\d+) t=([\d.]+)", l)
        if m: ph = m.group(1); phase = dict(ph=ph, t0=float(m.group(3)), fr=int(m.group(2)))
        m = re.match(r"MSNCAM end t=([\d.]+)", l)
        if m and phase: cur = dict(phase, end=float(m.group(1)), best=(9e9, 0.0), last=None); ev.append(cur); phase = {}
        m = re.match(r"CAMF t=([\d.]+) st=\d+ fov=[\d.]+ eye=([-\d.]+),([-\d.]+),([-\d.]+) look=\S+ homer=([-\d.]+),([-\d.]+),([-\d.]+)", l)
        if m:
            t, ex, ey, ez, hx, hy, hz = map(float, m.groups()); dd = math.dist((ex, ey, ez), (hx, hy + 1.5, hz))
            for e in ev:
                if e["end"] - 0.1 <= t <= e["end"] + 1.5:
                    if e["last"] is None: e["last"] = dd
                    if t >= e["end"] and dd < e["best"][0]: e["best"] = (dd, t - e["end"])
    return ev
bad = 0
for d in sys.argv[1:]:
    for e in run(d):
        ok = e["best"][0] >= MIN_OK; bad += not ok
        print(f"{d.split('/')[-1]} {e['ph']}: {e['fr']} кадров, конец {e['end']:.1f} с, у конца облёта {e['last'] if e['last'] is not None else float('nan'):.1f} м, ближе всего {e['best'][0]:.2f} м через {e['best'][1]:.2f} с  {'OK' if ok else 'БЛИЗКО'}")
sys.exit(1 if bad else 0)
