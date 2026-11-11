#!/usr/bin/env python3
"""Будка-93 / 1233 T02: смена уровня при отказе экрана загрузки (scrfail1065 4). check_lvl.py smoke.txt new|old
new: SCROPEN1065 rc=-3->-3 дважды (обе попытки), LOADCORD1233 (шнур вместо чёрного экрана), уровень загружен (LEVEL ev=load n=4). old (красный): шнура нет."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
s = next(i for i, l in enumerate(L) if l.startswith('LEVELUP'))
T = L[s:]
def cnt(p): return sum(1 for l in T if re.search(p, l))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
chk('экран не открылся и после выгрузки (SCROPEN1065 rc=-3->-3 x2)', cnt(r'SCROPEN1065 .*rc=-3->-3') == 2)
chk('уровень загружен (LEVEL ev=load n=4)', cnt(r'^LEVEL ev=load n=4'))
if mode == 'new': chk('шнур вместо чёрного экрана (LOADCORD1233)', cnt(r'^LOADCORD1233') == 1)
else: chk('красный: шнура нет', cnt(r'^LOADCORD1233') == 0)
wd = [l for l in T if l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l)]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
chk('WATCHDOG 0: %d' % len(wd), not wd); chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
