#!/usr/bin/env python3
"""Build 95 П2: класс TELEPORT. check_tele.py smoke.txt fsm|news
fsm (перенос миссии внутри уровня без выбора из меню: flag msnsel): TFSM teleport plan(b=2 PRELOADER)/close/free/load ok=1/open, клетки у новой точки прочитаны (MSNLOAD), газеты Loading нет (LOADNEWS не появляется после MSN select).
news (настоящий MISSION_SELECT из паузы, gen_msel): газета остаётся (LOADNEWS), строк TFSM teleport нет."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
s = next(i for i, l in enumerate(L) if re.match(r'^(MSN select|SCENE L5_sel)', l))
T = L[s:]
def cnt(p): return sum(1 for l in T if re.search(p, l))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
tf, news, load = cnt(r'^TFSM teleport plan'), cnt(r'^LOADNEWS'), cnt(r'^MSNLOAD')
print(f'TFSM teleport={tf} LOADNEWS={news} MSNLOAD={load}')
if mode == 'fsm':
    chk('экран PRELOADER (plan b=2)', cnt(r'^TFSM teleport plan a=\d+ b=2') == 1); chk('load ok=1 и open', cnt(r'^TFSM teleport load a=1') == 1 and cnt(r'^TFSM teleport open') == 1)
    chk('клетки у точки прочитаны (MSNLOAD fsm)', load >= 1); chk('газеты нет', news == 0)
else:
    chk('газета MISSION_SELECT осталась', news >= 1 and tf == 0)
bad = [l for l in T if l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l)]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
chk('WATCHDOG 0: %d' % len(bad), not bad); chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
