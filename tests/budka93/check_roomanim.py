#!/usr/bin/env python3
"""Build 95 (Данные-93, ROOMANIMGT95): анимация текстур комнаты на игровом времени. check_roomanim.py smoke.txt green|red
Строки `ROOMANIM stat ... lag=` (lag = smkT − время анимации): green (flag roomanimgt95 1) — после паузы lag вырос на ~3 с (пауза не считается), red (flag roomanimgt95 0) — lag постоянен (анимация шла на паузе)."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
scene = ''; lag = {}
for l in L:
    m = re.match(r'^SCENE (\S+)', l)
    if m: scene = m.group(1)
    m = re.match(r'^ROOMANIM stat .*?lag=(-?[\d.]+)', l)
    if m and scene in ('G_before', 'G_after'): lag[scene] = float(m.group(1))
print('lag before/after:', lag.get('G_before'), lag.get('G_after'))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
chk('есть замеры lag до и после паузы', 'G_before' in lag and 'G_after' in lag)
if 'G_before' in lag and 'G_after' in lag:
    d = lag['G_after'] - lag['G_before']
    if mode == 'green': chk('после паузы lag вырос на ~3 с (пауза не идёт в анимацию): %.2f' % d, 2.0 <= d <= 4.5)
    else: chk('красный: lag постоянен (анимация шла на паузе): %.2f' % d, abs(d) < 0.8)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
