#!/usr/bin/env python3
"""Лестница памяти (правка 435): график наибольшего цельного куска кучи во времени и сводка ступеней по журналу(ам) смоука.
   tools/memladder_report.py smoke.txt [ещё.txt ...]   — каждый файл отдельной строкой графика (например «до» без лестницы и «после»)
Читает MEMBLK (раз в 5 с игры: blk, free, min, dd, q) и WATCHDOG mem (ступени 1..5, кто просил). Только чтение."""
import re, sys
BARS = ' ▁▂▃▄▅▆▇█'
for f in sys.argv[1:]:
    t = open(f, 'rb').read().decode('utf-8', 'replace').split('\n')
    pts = [(float(m.group(2)), float(m.group(1))) for l in t for m in [re.search(r'MEMBLK blk=([\d.]+) .* t=([\d.]+)', l)] if m]
    steps = {k: 0 for k in range(1, 6)}; who = {}
    for l in t:
        m = re.search(r'WATCHDOG mem who=(\S+) size=\d+ step=(\d)', l)
        if m: steps[int(m.group(2))] += 1; who[m.group(1)] = who.get(m.group(1), 0) + 1
    ys = [y for _, y in pts]; mx = max(ys) if ys else 1
    line = ''.join(BARS[min(8, int(8 * y / mx + .5))] for y in ys)
    print(f"{f}\n  точек {len(ys)}  наибольший кусок МБ: мин {min(ys) if ys else '-'}  макс {mx if ys else '-'}\n  {line}\n  ступени 1..5: {steps}  просили: {who}")
