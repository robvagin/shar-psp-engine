#!/usr/bin/env python3
"""Гейт 107б: карточка миссии с картинкой при залипшем дескрипторе и неудачных переоткрытиях. check.py ОТЧЁТ [OLD]
  OLD=0 зелёный: MSN briefing-loaded ... lost=0 и в ящике art-io + art-reopen; OLD=1 (прежнее после 108) обязан быть красным: lost=1."""
import re, sys
from pathlib import Path
log = (Path(sys.argv[1]) / 'smoke.txt').read_text(errors='replace'); old = len(sys.argv) > 2 and sys.argv[2] == '1'
m = re.search(r'MSN briefing-loaded .*wait=([\d.]+) .*pic=(\S+) lost=(\d)', log)
io = len(re.findall(r'^BBOP art-io', log, re.M)); ro = len(re.findall(r'^BBOP art-reopen', log, re.M))
if not m: print('FAIL: нет строки MSN briefing-loaded'); sys.exit(1)
if m.group(3) != '0': print('FAIL: карточка без картинки (pic=%s lost=1, ждала %s с; art-io %d, art-reopen %d)' % (m.group(2), m.group(1), io, ro)); sys.exit(1)
if io < 1: print('FAIL: отказ чтения не сработал (art-io 0): модель залипшего дескриптора не включилась'); sys.exit(1)
print('OK: карточка с картинкой %s lost=0 (ждала %s с; art-io %d, art-reopen %d)' % (m.group(2), m.group(1), io, ro))
