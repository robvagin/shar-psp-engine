#!/usr/bin/env python3
"""№1063 семейство: по первой карточке каждого уровня L1–L7 векторное пятно 1,6 м нарисовано (CARDSPOT в 1 м по плану от карточки, полуширина 0,80 ±0,02). python3 check_family.py out_n [--expect fail]  (прогон family.txt; красный — family_old.txt: пятен нет)"""
import json, math, re, sys, os
a = [x for x in sys.argv[1:] if not x.startswith('--')]; exp_fail = '--expect' in sys.argv and sys.argv[sys.argv.index('--expect') + 1] == 'fail'
cards = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'family_cards.json')))
s = open(a[0] + '/smoke.txt', errors='replace').read()
spots = [(float(m.group(1)), float(m.group(3)), float(m.group(4))) for m in re.finditer(r'^CARDSPOT n=\d+ a=\S+ c=(\S+?),(\S+?),(\S+?) half=(\S+),\S+', s, re.M)]
spots = [(x, z, h) for x, y, z, h in [(float(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))) for m in re.finditer(r'^CARDSPOT n=\d+ a=\S+ c=(\S+?),(\S+?),(\S+?) half=(\S+),\S+', s, re.M)]]
ok = True
for lv in sorted(cards, key=int):
    x, y, z = cards[lv]; hit = [h for sx, sz, h in spots if math.hypot(sx - x, sz - z) < 1.0]
    good = len(hit) >= 1 and all(abs(h - .8) <= .02 for h in hit); ok = ok and good
    print(('  ok  ' if good else '  FAIL ') + f'L{lv} карточка ({x}; {z}): пятен {len(hit)}, полуширина {hit[0] if hit else "-"} м (цель 0,80)')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok != exp_fail else 1)
