#!/usr/bin/env python3
"""№1097: по smoke.txt (строки MENUITEM экрана провала, scr=4) выбранный пункт колеблется: масштаб s от ≤ 0,96 до ≥ 0,999, не меньше 20 отсчётов на пункт, пункт 0 и пункт 1.
Красный контроль: прогон со сборкой -DFAILPULSE1097_OLD=1 и flag failynpulse0 даёт s = 1,0000 на всех строках.
  failpulse_check.py build/smoke/<время>/smoke.txt"""
import re, sys
rows = {}
for l in open(sys.argv[1], errors='replace'):
    m = re.search(r'MENUITEM .*scr=4 mode=0 sel=(\d+) ms=([\d.]+) s=([\d.]+)', l)
    if m: rows.setdefault(int(m.group(1)), []).append((float(m.group(2)), float(m.group(3))))
bad = []
for sel in (0, 1):
    r = rows.get(sel, [])
    lo = min((s for _, s in r), default=9); hi = max((s for _, s in r), default=0)
    print('пункт %d: отсчётов %d, масштаб от %.4f до %.4f' % (sel, len(r), lo, hi))
    if len(r) < 20 or lo > 0.96 or hi < 0.999: bad.append(sel)
print('FAIL: не пульсирует пункт(ы) %s' % bad if bad else 'OK')
sys.exit(1 if bad else 0)
