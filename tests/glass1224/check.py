#!/usr/bin/env python3
# №1224: check.py old|new|kick SMOKE — стекло L4 (-244.5, -91.5), подход с +z. Журнал BENCH1034 flag benchlog 1, сцены G_old / G_new / G_kick.
#   old: красный контроль — в фазе «вперёд» проходит насквозь (z <= -92.5); new: упирается (z не меньше -91.5 + 0.3) и стекло цело; kick: после пинка проходит (z <= -92.5)
import re, sys
mode, f = sys.argv[1], sys.argv[2]
txt = open(f, errors='ignore').read()
rows = [(float(m[1]), float(m[2]), float(m[3]), float(m[4])) for m in re.finditer(r'BENCH1034 t=(\S+) x=(\S+) y=(\S+) z=(\S+) g=', txt)]
sc = {m[1]: float(m[2]) for m in re.finditer(r'SCENE (G_(?:old|new|kick)) (\S+)', txt)}
if 'G_' + mode not in sc: print('нет сцены', mode); sys.exit(2)
t0 = sc['G_' + mode]; ph = [r for r in rows if t0 + 2.5 <= r[0] < t0 + 9.0]
if not ph: print('мало строк'); sys.exit(2)
zmin = min(r[3] for r in ph); print('%s: самая дальняя точка Гомера z = %.2f (стекло на -91.5; <= -92.5 — прошёл насквозь, >= -91.2 — упёрся)' % (mode, zmin))
if mode == 'new': sys.exit(0 if zmin >= -91.2 else 1)
sys.exit(0 if zmin <= -92.5 else 1)
