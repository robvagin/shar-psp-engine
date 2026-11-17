#!/usr/bin/env python3
"""Гейт №1269: на карточке миссии есть картинка. Читает smoke.txt прогона tests/briefpic1269/scenario.txt.
  check.py ОТЧЁТ    зелёный: MSN briefing-loaded ... pic=mis01_00 lost=0; при сборке -DBRIEFPIC1269_OLD=1 обязан быть красным (lost=1)"""
import re, sys
from pathlib import Path
log = (Path(sys.argv[1]) / 'smoke.txt').read_text(errors='replace')
m = re.search(r'MSN briefing-loaded .*wait=([\d.]+) .*pic=(\S+) lost=(\d)', log)
if not m: print('FAIL: нет строки MSN briefing-loaded'); sys.exit(1)
if m.group(3) != '0': print('FAIL: карточка без картинки (pic=%s lost=1, ждала %s с)' % (m.group(2), m.group(1))); sys.exit(1)
print('OK: карточка с картинкой %s lost=0 (LOADING держалась %s с, память не находилась первые запуски)' % (m.group(2), m.group(1)))
