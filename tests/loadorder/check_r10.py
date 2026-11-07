#!/usr/bin/env python3
"""R10: после «flag roomsync» (сцена LO10_SYNC) Гомер обязан стоять в комнате на её полу (room=SimpsonsHouse, y -20 +-0,6) во всех отсчётах HOMER. Код 0 — стоит, 1 — остался в городе под землёй."""
import re, sys
L = open(sys.argv[1], errors='replace').read().splitlines()
on, n, bad = False, 0, []
for i, l in enumerate(L, 1):
    if l.startswith('SCENE'): on = l.split()[1] == 'LO10_SYNC'; continue
    m = re.match(r'HOMER t=([\d.]+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) h=\S+ room=(\S+)', l)
    if on and m:
        n += 1
        y, room = float(m.group(3)), m.group(5)
        if room != 'SimpsonsHouse' or abs(y + 20.0) > 0.6: bad.append((i, room, y))
print(f'== {sys.argv[1]}: отсчётов после roomsync {n}, не в комнате или не на полу {len(bad)}')
for i, room, y in bad[:5]: print(f'   ОПАСНО   стр.{i} room={room} y={y:.2f}')
sys.exit(1 if bad or not n else 0)
