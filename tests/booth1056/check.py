#!/usr/bin/env python3
"""№1056: разбор smoke.txt матрицы будки. check.py smoke.txt L -> строка таблицы; код 0, если на уровне L приехала каждая машина списка
(ok=1), кроме своей машины уровня (она — обычный вызов, ok=0), и будка открывалась оба раза (PHONE view open), без open-fail."""
import re, sys
own = {1: 'famil_v', 2: 'honor_v', 3: 'lisa_v', 4: 'marge_v', 5: 'apu_v', 6: 'bart_v', 7: 'homer_v'}
sm, L = sys.argv[1], int(sys.argv[2])
t = open(sm, errors='replace').read().splitlines()
res = {}
for l in t:
    m = re.match(r'PHONECAR (\S+) ok=(\d) ', l)
    if m: res[m.group(1)] = int(m.group(2))
bad = [n for n, ok in res.items() if ok != (0 if n == own[L] else 1)]
opens = sum(1 for l in t if l.startswith('PHONE view open'))
fails = sum(1 for l in t if 'open-fail' in l or 'nomodel' in l)
heap = []
for l in t:
    m = re.match(r'BB\s+\d+\s+\S+\s+L\d.*?heap\s+([\d.]+)', l)
    if m: heap.append(float(m.group(1)))
hb = [re.search(r'free=(\d+)', l).group(1) for l in t if l.startswith('HEAPB') and 'free=' in l]
hmin = min(heap) if heap else -1
hend = int(hb[-1]) / 1048576 if hb else -1
print(f'  L{L}: машин {len(res)}/42, не приехали (кроме своей {own[L]}): {bad if bad else "нет"}; будка открылась {opens}/2, отказов {fails}; куча свободно: мин {hmin:.2f} МБ, в конце {hend:.2f} МБ')
sys.exit(0 if len(res) == 42 and not bad and opens == 2 and not fails else 1)
