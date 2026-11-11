#!/usr/bin/env python3
"""Build 95 П2: DEATH/BUSTED, клетки у точки возврата под закрытым экраном. check_death.py smoke.txt new|old (сценарий tests/loadorder/r9_death_l1.txt; old: flag deathring1239old 1)
new: на каждую смерть строка ROOMRING1209 death, первая читает клетки (cells>0); old (красный): строк нет. Ни WATCHDOG nofloor/belowworld."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
d = sum(1 for l in L if l.startswith('DEATH ')); r = [l for l in L if l.startswith('ROOMRING1209 death')]
first = int(re.search(r'cells=(\d+)', r[0]).group(1)) if r else -1
print(f'DEATH={d} ROOMRING death={len(r)} первая cells={first}')
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
chk('смерти были', d >= 1)
if mode == 'new': chk('кольцо у точки возврата на каждую смерть, первая читает клетки', len(r) == d and first > 0)
else: chk('красный: колец нет', len(r) == 0)
chk('нет nofloor/belowworld', not any(re.search(r'WATCHDOG (nofloor|belowworld)', l) for l in L)); chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
