#!/usr/bin/env python3
# №1230: check.py old|new SMOKE — сцены R_old / R_new из rail_L5.txt (строки BENCH1034, flag benchlog 1). Стена x = -241.38, вдоль +z.
#   old: красный контроль — после касания (x <= -240.9) вдоль стены идёт не быстрее 3,45 м/с (половина идеала; на хосте tests/rail1230/gate.sh при 60° — стоит)
#   new: скользит вдоль стены не медленнее 4.5 м/с (идеал 8·cos30° = 6.9), сквозь не проходит (x не меньше -241.38 + 0.15), остаётся на площадке (y > 27)
import re, sys
mode, f = sys.argv[1], sys.argv[2]
txt = open(f, errors='ignore').read()
rows = [(float(m[1]), float(m[2]), float(m[3]), float(m[4])) for m in re.finditer(r'BENCH1034 t=(\S+) x=(\S+) y=(\S+) z=(\S+) g=', txt)]
sc = {m[1]: float(m[2]) for m in re.finditer(r'SCENE (R_(?:old|new)) (\S+)', txt)}
k = 'R_' + mode
if k not in sc: print('нет сцены', k); sys.exit(2)
t0 = sc[k]; end = min([v for n, v in sc.items() if v > t0] + [t0 + 9.0])
r = [x for x in rows if t0 + 2.5 <= x[0] < end]
if len(r) < 20: print('мало строк', len(r)); sys.exit(2)
tc = next((x[0] for x in r if x[1] <= -240.9), None)
if tc is None: print('до стены не дошёл'); sys.exit(2)
a = [x for x in r if tc + .5 <= x[0] <= tc + 1.5]
if len(a) < 5: print('мало строк после касания'); sys.exit(2)
v = (a[-1][3] - a[0][3]) / (a[-1][0] - a[0][0])
minx = min(x[1] for x in r); miny = min(x[2] for x in r[len(r) // 3:])
print('%s: касание на t=%.2f, вдоль стены %.2f м/с (идеал 6.9), x min %.2f (стена -241.38), y min %.2f' % (mode, tc - t0, v, minx, miny))
if mode == 'old': sys.exit(0 if v <= 3.45 else 1)
sys.exit(0 if v >= 4.5 and minx >= -241.38 + .15 and miny > 27 else 1)
