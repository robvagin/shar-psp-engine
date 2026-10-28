#!/usr/bin/env python3
"""№1037: таблица «было / стало» переноса всех текстов заданий L1–L7 (host.c: uiWrap из viewer/ui_layout.h, ширины букв pix8). python3 table.py [--bad]"""
import subprocess, re, sys
def run(w, m):
    out = subprocess.run(['./host', str(w), str(m)], stdin=open('texts.txt'), capture_output=True, text=True).stdout.strip().split('\n')
    return [l.split('\t') for l in out]
ADV = [int(x) for x in re.search(r'\{(.*)\}', open('advtab.h').read()).group(1).split(',')]
lw = lambda s: sum(ADV[ord(c)] if ord(c) < 128 else ADV[63] for c in s)
texts = [l.split('\t')[1].strip() for l in open('texts.txt') if '\t' in l]
import os
RED = os.environ.get('RED') == '1'   # красный контроль: «стало» считается прежним переносом
rows = []; bad = 0
for w in (209, 181, 150, 120, 108):
    o = run(w, 0); n = run(w, 0 if RED else 1); sh = worse = wo = wn = ovo = ovn = 0; ex = []
    for t, a, b in zip(texts, o, n):
        lo, ln = int(a[1]), int(b[1])
        if ln < lo: sh += 1; ex.append((a[0], lo, ln, a[2], b[2]))
        elif ln > lo: worse += 1
        for r, which in ((a, 'o'), (b, 'n')):
            ls = r[2].split(' / '); wd = len(ls) >= 2 and ' ' not in ls[-1] and ' ' in ls[-2]; ov = any(lw(x) > w for x in ls)
            if which == 'o': wo += wd; ovo += ov
            else: wn += wd; ovn += ov
    rows.append((w, len(o), sh, worse, wo, wn, ovo, ovn))
    if '--ex' in sys.argv and w in (120, 108):
        print('ширина', w, 'стало короче:'); [print('   ', e) for e in ex[:12]]
print('ширина | текстов | стало короче | стало длиннее | висячих было | висячих стало | вылезов было | вылезов стало')
for r in rows: print(*r, sep=' | ')
worse_total = sum(r[3] for r in rows); ov_new = sum(r[7] for r in rows)
sea = [subprocess.run(['./host', '181', str(m)], stdin=open('texts.txt'), capture_output=True, text=True).stdout for m in (0, 0 if RED else 1)]
sl = lambda t: int(next(l for l in t.split('\n') if 'SEA' in l and 'CAPTAIN' in l).split('\t')[1])
sea_o, sea_n = sl(sea[0]), sl(sea[1])
ok = worse_total == 0 and ov_new <= sum(r[6] for r in rows) and sum(r[2] for r in rows) >= 10 and rows[-1][5] <= rows[-1][4] and sea_o == 3 and sea_n == 2
print('«TALK TO THE SEA CAPTAIN» при ширине 181: было %d строк, стало %d' % (sea_o, sea_n))
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
