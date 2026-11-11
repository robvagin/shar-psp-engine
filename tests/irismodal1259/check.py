#!/usr/bin/env python3
"""Будка-93 (1259): python3 tests/irismodal1259/check.py ПАПКА_ПРОГОНА green|red. Журнал smoke.txt + снимки r*.png сценария."""
import sys, re, glob, os
from PIL import Image, ImageStat
d = sys.argv[1]; mode = sys.argv[2]
L = open(os.path.join(d, 'smoke.txt'), errors='replace').read().splitlines()
fail = [i for i, l in enumerate(L) if l.startswith('MSN failed')]
mod = [l for l in L if l.startswith('IRISMODAL1259')]
wd = [l for l in L if l.startswith('WATCHDOG iris stuck')]
hold = [l for l in L if l.startswith('IRIS why=seat phase=hold')]
shots = sorted(glob.glob(os.path.join(d, 'r*.png')))
lum = [ImageStat.Stat(Image.open(p).convert('L')).mean[0] for p in shots]
late = lum[len(lum) // 2:] if lum else []
dark = sum(1 for v in late if v < 3)
print('провал миссии: %d, IRISMODAL1259: %d, WATCHDOG iris stuck: %d, seat-hold кадров: %d, снимков %d (чёрных из второй половины %d из %d)' % (len(fail), len(mod), len(wd), len(hold), len(shots), dark, len(late)))
if mode == 'green': ok = bool(fail) and len(mod) >= 1 and not wd and len(hold) < 90 and dark <= len(late) // 4
else: ok = bool(fail) and not mod and (len(hold) > 300 or dark > len(late) // 2)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
