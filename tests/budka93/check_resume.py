#!/usr/bin/env python3
"""Будка-93 / 1234: RESUME после записи на вопросе конца уровня 1. check_resume.py smokeB.txt new|old (запуск B: flag menu, flag saves, ✕ RESUME)
new: сразу загружен уровень 2 (LEVEL ev=load n=2), уровня 1 не грузили, повторного LEVELUP нет. old (красный): загружен L1, потом LEVELUP 1->2 и вторая загрузка."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
def cnt(p): return sum(1 for l in L if re.search(p, l))
l1, l2, up = cnt(r'^LEVEL ev=load n=1 '), cnt(r'^LEVEL ev=load n=2 '), cnt(r'^LEVELUP')
print(f'LEVEL load n=1:{l1} n=2:{l2} LEVELUP:{up}')
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
chk('RESUME нашёл запись (SVSCAN latest=0)', cnt(r'^SVSCAN .* latest=0'))
if mode == 'new': chk('сразу L2: один LEVEL load n=2, L1 не грузили, LEVELUP нет', l2 == 1 and l1 == 0 and up == 0)
else: chk('красный: L1, потом LEVELUP 1->2 и L2', l1 >= 1 and up == 1 and l2 == 1)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
