#!/usr/bin/env python3
"""Будка-93 (1258): python3 tests/irisstuck1258/check.py SMOKE.txt green|red. green: строка WATCHDOG iris stuck есть, после неё IRIS ... phase=open. red: строки нет и ирис так и стоит закрытым (hold без open)."""
import sys, re
L = open(sys.argv[1], errors='replace').read().splitlines(); mode = sys.argv[2]
w = [i for i, l in enumerate(L) if l.startswith('WATCHDOG iris stuck')]
# конец сценария: IRIS строки room_in после нажатия
ir = [(i, l) for i, l in enumerate(L) if l.startswith('IRIS why=room_in')]
opened = [i for i, l in ir if 'phase=open' in l]
held = [i for i, l in ir if 'phase=hold' in l]
print('WATCHDOG iris stuck: %d, IRIS room_in hold-строк %d, open-строк %d' % (len(w), len(held), len(opened)))
if mode == 'green':
    ok = len(w) >= 1 and any(i > w[0] for i in opened)
else:
    ok = len(w) == 0 and len(held) > 400   # без сторожа ирис стоит закрытым, пока сценарий не вернул dt (16 с = ~490 кадров против ~240 при сторожа 8 с)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
