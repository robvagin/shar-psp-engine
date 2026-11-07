#!/usr/bin/env python3
"""Правка 435 (лестница памяти) и 432 (MISSION SELECT): проверка одного прогона смоука. check.py smoke.txt РЕЖИМ
  starve — тесное место при открытии меню: ни одной неудачной попытки пакета (MSNSEL openfail), меню строками (rows=1), WATCHDOG mem — только записи ступеней
  fuzz   — бот меню: итог MSNSELFUZZ есть, шагов ≥ 200, нарушений инвариантов 0, открытий запасным видом 0
  rows   — MSNSELROWS: показанные строки = посчитанным заново по маскам, на ≥ 3 уровнях, пакет открыт
Печатает цифры и последнюю строку PASS / FAIL. Только чтение."""
import re, sys
t = open(sys.argv[1], 'rb').read().decode('utf-8', 'replace'); mode = sys.argv[2]
L = t.split('\n'); ok = True
def cnt(p): return sum(1 for l in L if re.search(p, l))
def say(k, v, good=None):
    global ok
    if good is False: ok = False
    print(f"  {k}: {v}{'' if good is None else ('  ok' if good else '  ПРОВАЛ')}")
blk = [float(m.group(1)) for l in L for m in [re.search(r'MEMBLK blk=([\d.]+)', l)] if m]
mn = [float(m.group(1)) for l in L for m in [re.search(r'MEMBLK .* min=([\d.]+)', l)] if m]
say('наибольший кусок (МБ): последний/минимум по журналу', f"{blk[-1] if blk else '-'}/{min(mn) if mn else '-'}")
steps = {k: cnt(r'WATCHDOG mem .* step=%d ' % k) for k in range(1, 6)}
say('WATCHDOG mem по ступеням 1..5', steps)
bad = cnt(r'MSNSELBAD'); say('MSNSELBAD', bad, bad == 0)
if mode == 'starve':
    of = cnt(r'MSNSEL openfail'); say('неудачных попыток пакета (MSNSEL openfail)', of, of == 0)
    r1 = cnt(r'MSNSEL open rows=1'); say('меню строками (rows=1)', r1, r1 >= 1)
    take = cnt(r'MEMLAD mem who=menu-res'); say('отложенный блок отдан (MEMLAD menu-res take)', take)
elif mode == 'fuzz':
    m = [re.search(r'MSNSELFUZZ seq=(\d+) steps=(\d+) opened=(\d+) bad=(\d+) fixes=(\d+) fallback=(\d+)', l) for l in L]; m = [x for x in m if x]
    say('итог бота MSNSELFUZZ', 'есть' if m else 'нет', bool(m))
    if m:
        seq, st, op, bd, fx, fb = map(int, m[-1].groups()); say('последовательностей/шагов/открытий', f"{seq}/{st}/{op}", st >= 200 and op >= 20); say('нарушений инвариантов', bd, bd == 0); say('открытий запасным видом', fb, fb == 0)
elif mode == 'break':   # отрицательный контроль самопроверки: 5 видов порчи состояния меню (flag msnselbreak 1..5) — каждый ловится и чинится
    br = cnt(r'MSNSELBREAK kind'); ck = cnt(r'MSNSELCHK'); wd = cnt(r'WATCHDOG msnsel')
    say('порч подброшено (MSNSELBREAK)', br, br >= 5); say('поймано и починено (MSNSELCHK)', ck, ck >= br); say('строк WATCHDOG msnsel', wd, wd >= br)
elif mode == 'rows':
    R = [re.search(r'MSNSELROWS lv=(\d+) rows=(\d) disp=(\S+) exp=(\S+)', l) for l in L]; R = [x for x in R if x]
    say('строк MSNSELROWS', len(R), len(R) >= 3); say('уровней', sorted({x.group(1) for x in R}), len({x.group(1) for x in R}) >= 3)
    dif = [x.group(0) for x in R if x.group(3) != x.group(4) or x.group(2) != '1']; say('расхождений показанного с ожидаемым', len(dif), not dif)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
