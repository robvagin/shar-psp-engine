#!/usr/bin/env python3
"""Проверка прогонов tests/missions/race_rivals.sh (правка Роба 1205, соперничество в уличных гонках). Источник — строки RACEPROG в smoke.txt (раз в секунду гонки:
on=1 игрок в машине, px pz — где он; у каждого соперника x z v wp rk d a f и W, если он ждёт игрока).
  race_rivals.py seat smoke.txt X Z     основная фаза гонки УРОВНЯ 5 (sr3) сажает игрока за руль: первая строка гонки — on=1, игрок в 6 м от локатора sr3_carstart (X Z), а соперник за 40 с
                                       гонки уехал со старта больше чем на 100 м (прежнее: игрок пешком на 588 м от старта, соперник ждёт его на ручнике и не двигается)
  race_rivals.py jam smoke.txt NAME N  соперник NAME (L7 sr2: cHears) не заклинивает: за гонку набрал точку пути N и больше (прежнее: 0 точек, качается у забора ±3 м 190 с)
Код 0 — зелёное, 1 — красное."""
import math, re, sys

def rows(path):
    return [ln for ln in open(path, errors='replace') if ln.startswith('RACEPROG ')]

def tval(ln):
    return float(re.search(r't=([\d.]+)', ln).group(1))

def seat(path, cx, cz):
    r = rows(path)
    if not r:
        print('FAIL: нет строк RACEPROG (гонка не началась)'); return 1
    first = r[0]
    f = dict(re.findall(r'(\w+)=(-?[\d.]+)', first.split(' ', 2)[2]))
    on, px, pz = int(f['on']), float(f['px']), float(f['pz'])
    d = math.hypot(px - cx, pz - cz)
    r0 = re.search(r' (\w+) x=(-?\d+) z=(-?\d+) v=', first)
    t0 = tval(first); moved = 0.0
    if r0:
        name, x0, z0 = r0.group(1), float(r0.group(2)), float(r0.group(3))
        for ln in r:
            if tval(ln) - t0 > 40: break
            m = re.search(r' %s x=(-?\d+) z=(-?\d+) v=' % name, ln)
            if m: moved = max(moved, math.hypot(float(m.group(1)) - x0, float(m.group(2)) - z0))
    ok = on == 1 and d <= 6.0 and moved > 100.0
    print('игрок %s, от локатора %.1f м; соперник за 40 с уехал со старта на %.0f м -> %s' % ('в машине' if on else 'ПЕШКОМ', d, moved, 'PASS' if ok else 'FAIL'))
    return 0 if ok else 1

def jam(path, name, need):
    r = rows(path)
    if not r:
        print('FAIL: нет строк RACEPROG (гонка не началась)'); return 1
    best = 0; xs = []
    for ln in r:
        m = re.search(r' %s x=(-?\d+) z=(-?\d+) v=-?\d+ wp=(\d+)' % name, ln)
        if m: best = max(best, int(m.group(3))); xs.append((float(m.group(1)), float(m.group(2))))
    span = max((math.hypot(a[0] - xs[-1][0], a[1] - xs[-1][1]) for a in xs[-40:]), default=0.0)
    ok = best >= need
    print('%s: набрал точек пути %d (нужно %d и больше), за последние 40 с разошёлся на %.0f м, гонка %.0f с -> %s' % (name, best, need, span, tval(r[-1]) - tval(r[0]), 'PASS' if ok else 'FAIL'))
    return 0 if ok else 1

if __name__ == '__main__':
    if sys.argv[1] == 'seat': sys.exit(seat(sys.argv[2], float(sys.argv[3]), float(sys.argv[4])))
    sys.exit(jam(sys.argv[2], sys.argv[3], int(sys.argv[4])))
