#!/usr/bin/env python3
"""№1160: цена ступени Car Aim из smoke.txt прогона caraim1160_cost_scenario.txt: средние по секундам езды на ступени 2 и на 4.
Строка smoke.txt: t scene fps work workmax cpu tris draws ramMB mode missed ...  (work, cpu — мс; tris, draws — за кадр). Запуск: caraim1160_cost.py <папка прогона>"""
import sys, re
phases = {}; ph = 0
for l in open(sys.argv[1] + '/smoke.txt', errors='replace'):
    p = l.split()
    if l.startswith('SCENE drive'): ph += 1; continue
    if len(p) > 10 and p[1] == 'drive' and re.match(r'^\d+(\.\d+)?$', p[0]): phases.setdefault(ph, []).append((float(p[0]), float(p[2]), float(p[3]), float(p[5]), int(p[6]), int(p[7]), int(p[10])))
def stat(rs, nm):
    if not rs: print(nm, 'нет строк'); return
    avg = lambda i: sum(r[i] for r in rs) / len(rs)
    print('%-10s строк %2d  fps %5.1f  work %5.2f мс  cpu %5.2f мс  ктрис %6.2f  вызовов %6.1f  пропусков %d' % (nm, len(rs), avg(1), avg(2), avg(3), avg(4) / 1000, avg(5), sum(r[6] for r in rs)))
# фаза 1 — разогрев, 2 — ступень 2, 3 — ступень 4; первые 4 строки каждой езды (досев клеток после goto) не считаем
stat(phases.get(2, [])[4:], 'ступень 2'); stat(phases.get(3, [])[4:], 'ступень 4')
