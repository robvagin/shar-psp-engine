#!/usr/bin/env python3
"""Build 95 П2: машина переходов (transit_fsm.h), класс ROOM_IN. check_fsm.py smoke.txt ROOM N
N=0: порядок TFSM room_in plan, close, free, load ok=1, ready, open; вход состоялся. N=1..4 (flag tffail N): столько ступеней FREE_MORE (WATCHDOG mem ... fsm), потом load ok=1 и вход.
N=5 (отказ всех ступеней): TFSM simplify, входа нет, ирис снят (DOORIRIS1226 drop), игра не зависла (END). tfsmold: строк TFSM нет (прежний путь)."""
import sys, re
f, room, N = sys.argv[1], sys.argv[2], sys.argv[3]; NOFIT = 'nofit' in sys.argv[4:]
old = N == 'old'; N = 0 if old else int(N)
L = open(f, errors='replace').read().split('\n')
s = next(i for i, l in enumerate(L) if l.startswith('SCENE ' + room + '_door'))
T = L[s:]
def idx(p, st=0):
    for i in range(st, len(T)):
        if re.search(p, T[i]): return i
    return -1
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
tf = [l for l in T if l.startswith('TFSM room_in')]
steps = sum(1 for l in T if re.search(r'^WATCHDOG mem who=room_in .* fsm', l))
enter = idx(r'^ROOM ev=enter room=' + room)
print(f'TFSM строк={len(tf)} ступеней FREE_MORE={steps} вход={enter >= 0}')
if old: chk('прежний путь: строк TFSM нет, вход есть', not tf and enter >= 0)
else:
    pos = 0
    for st in ('plan', 'close', 'free', 'load'):
        i = idx(r'^TFSM room_in ' + st, pos); chk('TFSM ' + st, i >= 0); pos = max(pos, i)
    if N <= 4:
        chk('ступеней FREE_MORE ровно %d' % N, steps == N); chk('load ok=1', idx(r'^TFSM room_in load a=1') >= 0)
        chk('ready и open', idx(r'^TFSM room_in ready') >= 0 and idx(r'^TFSM room_in open') >= 0); chk('вход состоялся', enter >= 0)
    elif 'simpl' not in sys.argv[4:]:   # Build 96: все ступени отказали — чистая перезагрузка (вне миссии), потом комната читается и вход состоялся
        chk('ступень 5 reload (TFSM room_in reload)', idx(r'^TFSM room_in reload') >= 0)
        rl = [l for l in T if re.search(r'^WATCHDOG mem who=room_in .* step=5 reload=1 ', l)]; chk('WATCHDOG step=5 reload=1', len(rl) == 1)
        cr = next((l for l in T if l.startswith('CLEANRELOAD96 ms=')), ''); m = re.search(r'dxz=([\d.]+)', cr); chk('игрок там же (dxz<0,5 м): ' + (m.group(1) if m else '-'), bool(m) and float(m.group(1)) < 0.5)
        bs = next((l for l in T if l.startswith('BOOTH1060 msn same=')), ''); chk('прогресс и монеты те же (msn same=1)', 'same=1' in bs)
        chk('load a=1 после перезагрузки', idx(r'^TFSM room_in load a=1') >= 0); chk('вход состоялся, SIMPLIFY нет', enter >= 0 and idx(r'^TFSM room_in simplify') < 0)
        chk('один экран на вход (XITION room_in 1)', sum(1 for l in T if l.startswith('XITION room_in')) == 1)
        ms = re.search(r'ms=(\d+)', cr); print('время перезагрузки, мс:', ms.group(1) if ms else '-')
    else:
        chk('SIMPLIFY (честный отказ)', idx(r'^TFSM room_in simplify') >= 0); chk('входа нет' + ('' if NOFIT else ', ирис снят'), enter < 0 and (NOFIT or idx(r'^DOORIRIS1226 drop') >= 0))
if NOFIT:   # путь прелоадера при !fits (решение Роба 2): экран PRELOADER (b=2), город выброшен под ним, комната прочитана, прелоадер не короче 1,2 с
    chk('экран PRELOADER (plan b=2)', idx(r'^TFSM room_in plan a=\d+ b=2') >= 0)
    fr = idx(r'^TFSM room_in free a=([1-9]\d*)'); chk('FREE выбросил клетки города под прелоадером (free a>0)', fr >= 0)
    if N == 5: chk('SIMPLIFY не тихий: WATCHDOG step=5 simplify', idx(r'^WATCHDOG mem who=room_in .* step=5 simplify') >= 0)
bad = [l for l in T if l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l)]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
chk('WATCHDOG (кроме spawn hero, mem) 0: %d' % len(bad), not bad); chk('END', any(l == 'END' for l in L))
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
