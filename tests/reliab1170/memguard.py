#!/usr/bin/env python3
"""№1170: проверка, что четыре выделения при загрузке уровня подключены к mgCallocN / проверке (viewer/main_levels.h, main_collect.h): без этого test memguard.c доказывает только хелпер.
  memguard.py [корень]   -> RESULT: OK/FAIL. Красный контроль гейта (memguard.sh) — тот же разбор на копии, где проверки вырезаны."""
import re, sys
from pathlib import Path
R = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
lv = (R / 'viewer/main_levels.h').read_text(encoding='utf-8'); co = (R / 'viewer/main_collect.h').read_text(encoding='utf-8')
bad = []
if len(re.findall(r'mgCallocN\(3,', lv)) != 1: bad.append('main_levels.h: таблицы кусков (pc, carP, skyP) без mgCallocN(3, …)')
if len(re.findall(r'mgCallocN\(4,', lv)) != 1: bad.append('main_levels.h: таблицы клеток (cl, cellBigF, cdl, cdlMark) без mgCallocN(4, …)')
if len(re.findall(r'Not enough memory: (piece|cell) tables', lv)) != 2: bad.append('main_levels.h: нет честного экрана «Not enough memory» после отказа')
if not re.search(r'cxTex = calloc[^\n]*\n[^\n]*!cxTex[^\n]*LOADMEM1170_OLD', co): bad.append('main_collect.h: cxTex без проверки')
print('RESULT: ' + ('FAIL — ' + '; '.join(bad) if bad else 'OK — четыре выделения проверены')); sys.exit(1 if bad else 0)
