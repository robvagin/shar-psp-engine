#!/usr/bin/env python3
"""Круг 6, P04 W9: таблица ухода ос WASP_EVADE[уровень] в viewer/collect.c = строки AddBehaviour(beecamera, EVADE_PLAYER, мин, макс, ?, ?, км/ч) в scripts/missions/levelNN/leveli.mfk
(закомментированная строка // = у уровня ухода нет, 0/0/0). Диск Роба только на чтение. Код выхода 0 — совпадает. --red: в ожидаемое подставлена соседняя ошибка (км/ч уровня 2 = 8), обязано упасть.
  python3 tests/collect/evade_table.py [корень диска, по умолчанию hit-and-run-web/source/game]"""
import re, sys, os
args = [a for a in sys.argv[1:] if not a.startswith('--')]
root = args[0] if args else os.path.expanduser('~/Downloads/shar-psp/hit-and-run-web/source/game')
src = open(os.path.join(os.path.dirname(__file__), '../../viewer/collect.c'), encoding='utf-8').read()
m = re.search(r'WASP_EVADE\[8\]\[3\] = \{(.*?)\};', src, re.S)
rows = re.findall(r'\{([^{}]*)\}', m.group(1)); code = [[float(x.strip().rstrip('f')) for x in r.split(',')] for r in rows]
bad = 0
for lv in range(1, 8):
    f = os.path.join(root, 'scripts/missions/level%02d/leveli.mfk' % lv)
    line = next((l for l in open(f, encoding='latin-1') if 'EVADE_PLAYER' in l), None)
    if line is None or line.lstrip().startswith('//'): want = [0.0, 0.0, 0.0]
    else:
        a = re.findall(r'"([0-9.]+)"', line); want = [float(a[0]), float(a[1]), float(a[4])]   # мин, макс, скорость км/ч
    if '--red' in sys.argv and lv == 2: want[2] = 8.0
    ok = code[lv] == want; bad += not ok
    print('%s уровень %d: данные %s, код %s' % ('PASS' if ok else 'FAIL', lv, want, code[lv]))
print('RESULT', 'FAIL' if bad else 'PASS'); sys.exit(1 if bad else 0)
