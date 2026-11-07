#!/usr/bin/env python3
"""№1166: итог сцены = последняя строка CAMIN сцены (в каждой сцене их две: на 1 с и итог). Печатает сцены с «внутри» > 0 по каждому прогону. Запуск: camin_final.py <папка прогона> [...]"""
import re, sys
for d in sys.argv[1:]:
    fin = {}; cur = None; order = []
    for l in open(d + '/smoke.txt', errors='replace'):
        if l.startswith('SCENE '): cur = l.split()[1]
        m = re.match(r'CAMIN t=\S+ frames=(\d+) any=(\d+) eye=(\d+) near=(\d+) ground=(\d+) car=(\d+) person=(\d+)', l)
        if m and cur:
            if cur not in fin: order.append(cur)
            fin[cur] = tuple(int(x) for x in m.groups())
    bad = {k: v for k, v in fin.items() if v[1]}
    print('%s: сцен %d, кадров %d, «внутри» %d; сцены с «внутри»: %s' % (d.split('/')[-1], len(fin), sum(v[0] for v in fin.values()), sum(v[1] for v in fin.values()), ', '.join('%s=%d(глаз %d, машина %d, человек %d)' % (k, v[1], v[2], v[5], v[6]) for k, v in bad.items()) or 'нет'))
