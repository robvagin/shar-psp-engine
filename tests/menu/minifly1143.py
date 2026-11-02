#!/usr/bin/env python3
"""№1143: журнал смоука (smoke.txt) после выбора MINI GAME в главном меню: перелёт камеры к треку один. Ровно одна строка «FEFLYRANGE 880-913 s=…» и ни одной «FEFLY item=3 …»
(табличного перелёта 880–910); длительность — как у одного перелёта (13 кадров по 30 к/с … 34 кадра ≈ 1,1 с, не двойная). Код 1 — двойной или отсутствует.   python3 tests/menu/minifly1143.py smoke.txt"""
import re, sys
t = open(sys.argv[1], encoding='utf-8', errors='replace').read()
rng = re.findall(r'^FEFLYRANGE 880-913 s=([0-9.]+)', t, re.M); tab = re.findall(r'^FEFLY item=3 ', t, re.M)
print('диапазон 880-913: %d (%s), табличных item=3: %d' % (len(rng), ','.join(rng), len(tab)))
ok = len(rng) == 1 and not tab and float(rng[0]) < 1.6
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
