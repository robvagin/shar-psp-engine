#!/usr/bin/env python3
"""№1057/№1058: разбор smoke.txt. check.py booth|ms smoke.txt [old]
Без old: код 0, если новое поведение на месте. С old: код 0, если прогон показал ПРЕЖНЕЕ поведение (контроль, что выключатель выключает и гейт может упасть)."""
import re, sys
mode, sm = sys.argv[1], sys.argv[2]; old = len(sys.argv) > 3
t = open(sm, errors='replace').read().splitlines()
errs = []
if mode == 'booth':
    seg = {}; cur = None
    for l in t:
        m = re.match(r'SCENE (\S+) ', l)
        if m: cur = m.group(1); seg[cur] = 0
        elif l.startswith('PHONEV sel ') and cur: seg[cur] += 1
    tap, hr, hl, hd = (seg.get(k, -1) for k in ('TAP', 'HOLD_R', 'HOLD_L', 'HOLD_RIGHT'))
    print(f'  шагов листания: касания L и R {tap} (надо 2), зажатие R 2 с {hr}, зажатие L 2 с {hl}, зажатая ► 2 с {hd}')
    if tap != 2: errs.append('одиночное касание L / R не равно одному шагу')
    if hd < 5: errs.append('зажатая ► не повторяется (№706)')
    if old:
        if hr > 2 or hl > 2: errs.append('L R повторяются при выключателе')
    else:
        if hr < 6: errs.append('зажатие R не листает')
        if hl < 6: errs.append('зажатие L не листает')
else:
    ev = [l for l in t if l.startswith('MSNSEL level=')]
    dirs = [(re.search(r'dir=(-?\d)', l).group(1), 'moved=1' in l) for l in ev]
    print('  MSNSEL: ' + ' | '.join(re.sub(r' t=.*', '', l) for l in ev))
    want = [('1', True), ('-1', True)] * (1 if old else 2)   # R, L, ► ◄  (при выключателе — только ► ◄)
    if dirs != want: errs.append(f'листание уровней {dirs} вместо {want}')
for e in errs: print('  !! ' + e)
sys.exit(1 if errs else 0)
