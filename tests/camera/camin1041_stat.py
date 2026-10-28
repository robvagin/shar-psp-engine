#!/usr/bin/env python3
# №1041: статистика наездов из smoke.txt (MSNCAM start/end, MSNCAMP, SLOWFRAME, STREAMCOST). camin1041_stat.py SMOKE
import re, sys, math
L = open(sys.argv[1], errors='ignore').read().splitlines()
runs = []; cur = None
for l in L:
    m = re.match(r'MSNCAM start m(\d+) (\S+) phase=(\S+) frames=(\d+) t=([\d.]+)', l)
    if m: cur = dict(m=int(m[1]), cam=m[2], phase=m[3], nfr=int(m[4]), t0=float(m[5]), eye=[], hero=None, t1=None, slow=[], loads0=None, loads1=None); runs.append(cur); continue
    m = re.match(r'MSNCAMP t=([\d.]+) ct=\S+ phase=\S+ eye=(\S+),(\S+),(\S+) look=\S+ fovy=\S+ homer=(\S+),(\S+),(\S+) car=(\S+),(\S+)', l)
    if m and cur: cur['eye'].append((float(m[1]), float(m[2]), float(m[3]), float(m[4]))); cur['hero'] = (float(m[5]), float(m[6]), float(m[7])); cur['car'] = (float(m[8]), float(m[9])); continue
    m = re.match(r'MSNCAM end t=([\d.]+)', l)
    if m and cur: cur['t1'] = float(m[1]); continue
    m = re.match(r'SLOWFRAME t=([\d.]+) work=([\d.]+)', l)
    if m and cur and cur['t1'] is None: cur['slow'].append((float(m[1]), float(m[2])))
    m = re.match(r'STREAMCOST avg_us=\S+ max_us=(\d+) n=\d+ unl=\d+ loads=(\d+)', l)
    if m and cur:
        if cur['loads0'] is None: cur['loads0'] = int(m[2])
        if cur['t1'] is None or True: cur['loads1'] = int(m[2])
print('%-4s %-18s %5s %6s | %8s %8s %8s %8s | %6s %8s' % ('m', 'камера', 'кадров', 'длит.с', 'путь м', 'старт-герой м', 'макс от героя м', 'до героя в конце', 'тяж.кадров', 'клеток'))
for r in runs:
    e = r['eye']
    if len(e) < 2: print(r['m'], r['cam'], 'нет MSNCAMP'); continue
    path = sum(math.dist(e[i][1:], e[i - 1][1:]) for i in range(1, len(e)))
    hx = r['hero']; hd = lambda p: math.dist((p[1], p[3]), (hx[0], hx[2]))
    d0 = hd(e[0]); dmax = max(hd(p) for p in e); dend = hd(e[-1])
    dur = (r['t1'] or e[-1][0]) - r['t0']
    print('%-4d %-18s %6d %6.1f | %8.1f %13.1f %15.1f %16.1f | %10d %8s' % (r['m'], r['cam'], r['nfr'], dur, path, d0, dmax, dend, sum(1 for t, w in r['slow'] if w > 60), (r['loads1'] - r['loads0']) if r['loads0'] is not None else '?'))
