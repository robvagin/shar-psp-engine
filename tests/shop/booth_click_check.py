#!/usr/bin/env python3
"""Гейт №1266 (видео Роба 13.11): щелчки будки при ◄ ► и ✕ слышны. Читает smoke.txt прогона booth_scenario.txt:
строки `PHONEV click id=.. bus=ui|sfx lvl=..` пишет viewer/phone_view.c snd(). Слышно = шина меню (ui) с уровнем > 0.
  booth_click_check.py ОТЧЁТ     зелёный на новом коде; при сборке с -DBOOTHSND1266_OLD=1 (шина sfx, которую №1135 держит на нуле) обязан быть красным"""
import re, sys
from pathlib import Path
log = (Path(sys.argv[1]) / 'smoke.txt').read_text(errors='replace').splitlines()
cl = [l for l in log if l.startswith('PHONEV click')]
ok = [l for l in cl if re.search(r'bus=ui lvl=(0\.0*[1-9]|[1-9])', l)]
if len(cl) < 3: print('FAIL: щелчков будки %d (< 3: ►, ◄, ✕)' % len(cl)); sys.exit(1)
if len(ok) != len(cl): print('FAIL: не слышны %d из %d щелчков, например: %s' % (len(cl) - len(ok), len(cl), [l for l in cl if l not in ok][0])); sys.exit(1)
print('OK: щелчков будки %d, все на шине меню с уровнем > 0 (%s)' % (len(cl), cl[0]))
