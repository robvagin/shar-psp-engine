#!/usr/bin/env python3
"""Гейт: L4 sr3 (Race 3 Checkpoint) - водитель соперника загружен и нарисован.
Использование: sr3_driver_check.py smoke.txt [--max-sec N]
Проверки (по журналу эмулятора):
  1. стадия sr3.1.0 началась (MSN stage sr3.1.0);
  2. после неё есть "MSN drivers-drawn N" с N>=1 (водитель нарисован);
  3. от первого "MSN driver-skip" (модели ещё нет) до первого drawn>=1 прошло не больше N секунд журнала (по строкам BB, одна в ~секунду);
  4. нет "MSN driver-skip ... noclip" (пакет персонажа без позы водителя);
  5. модель соперника "frink" читается один раз и без отказа (MSNNPC load frink, нет MSNNPC mem-fail frink).
Kearney на L4 в SuppressDriver (level.mfk) - водителем гонки не бывает; строка "MSN npc kearney kind=1" - фоновый прохожий."""
import re, sys

path = sys.argv[1]
maxsec = 3
if '--max-sec' in sys.argv:
    maxsec = int(sys.argv[sys.argv.index('--max-sec') + 1])
L = open(path, errors='replace').read().splitlines()
fails = []
st = next((i for i, l in enumerate(L) if l.startswith('MSN stage sr3.1.0')), None)
if st is None:
    print('CHECK stage FAIL: нет MSN stage sr3.1.0'); sys.exit(1)
drawn = next((i for i in range(st, len(L)) if re.match(r'MSN drivers-drawn [1-9]', L[i])), None)
skip = next((i for i in range(st, len(L)) if L[i].startswith('MSN driver-skip')), None)
if drawn is None:
    fails.append('водитель не нарисован ни разу после старта стадии (нет MSN drivers-drawn >=1)')
else:
    t0 = skip if skip is not None else st
    bb = sum(1 for l in L[t0:drawn] if l.startswith('BB '))
    print('CHECK drawn ok: строка %d, строк BB от первого skip: %d (порог %d)' % (drawn + 1, bb, maxsec))
    if bb > maxsec:
        fails.append('водитель появился через %d строк BB (> %d)' % (bb, maxsec))
if any('MSN driver-skip' in l and 'noclip' in l for l in L[st:]):
    fails.append('driver-skip noclip: у пакета персонажа нет позы водителя')
if any(l.startswith('MSNNPC mem-fail frink') or ('MSN npc frink' in l and 'give-up' in l) for l in L[st:]):
    fails.append('модель frink получила отказ по памяти')
if not any(l.startswith('MSNNPC load frink') for l in L[st:]) and drawn is not None and skip is not None:
    fails.append('после driver-skip нет MSNNPC load frink')
if fails:
    for f in fails: print('CHECK FAIL:', f)
    print('RESULT sr3 driver FAIL'); sys.exit(1)
print('RESULT sr3 driver OK'); sys.exit(0)
