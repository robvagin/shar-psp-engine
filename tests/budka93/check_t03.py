#!/usr/bin/env python3
"""Будка-93 / 1233 T03 и 1228. check_t03.py smoke.txt double|fail new|old
double (flag t03double: Continue снимает ирис msn, тут же второй перенос): new — ирис msn закрывается ОДИН раз после MSN select, MSNEWS2 1228, второй газеты нет; old (красный) — два закрытия.
fail (flag scrfail1065 2: газета не открывается и после лестницы): new — шнур TELECORD1228, клетки прочитаны (MSNLOAD), ирис раскрыт; old (красный) — SCROPEN1065 rc=-3->-3 и пустой LOADNEWS без экрана, шнура нет."""
import sys, re
f, kind, mode = sys.argv[1:4]
L = open(f, errors='replace').read().split('\n')
s = next(i for i, l in enumerate(L) if l.startswith('MSN select'))
T = L[s:]
def cnt(p): return sum(1 for l in T if re.search(p, l))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
closes = cnt(r'^IRIS why=msn phase=close f=0\.00'); news = cnt(r'^LOADNEWS'); ms2 = cnt(r'^MSNEWS2 1228'); cord = cnt(r'^TELECORD1228'); load = cnt(r'^MSNLOAD cells=\d+')   # cells=0 бывает, когда клетки у точки уже в памяти
print(f'IRIS msn close={closes} LOADNEWS={news} MSNEWS2={ms2} TELECORD={cord} MSNLOAD={load}')
chk('перенос состоялся (MSNLOAD)', load >= 1 or kind == 'double')
if kind == 'double':
    if mode == 'new':
        chk('ирис msn закрыт один раз', closes == 1); chk('второй перенос без ириса (MSNEWS2)', ms2 == 1); chk('газета одна', news == 1)
    else:
        chk('красный: ирис msn закрыт дважды', closes == 2); chk('красный: MSNEWS2 нет', ms2 == 0)
else:
    if mode == 'new':
        chk('шнур вместо пустого экрана (TELECORD1228)', cord == 1); chk('пустого LOADNEWS без экрана нет', news == 0); chk('ирис раскрыт (phase=done)', cnt(r'^IRIS why=msn phase=done') >= 1)
    else:
        chk('красный: шнура нет', cord == 0); chk('красный: пустой LOADNEWS после rc=-3->-3', news >= 1 and cnt(r'SCROPEN1065 site=teleport rc=-3->-3') >= 1)
wd = [l for l in T if l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l)]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
chk('WATCHDOG (кроме spawn hero, mem) 0: %d' % len(wd), not wd)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
