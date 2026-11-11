#!/usr/bin/env python3
"""Будка-93 / Build 96: проверка газетного класса при принудительном отказе памяти. check_ext.py smoke.txt КЛАСС [new|old]
new (зелёное): TFSM КЛАСС plan, load a=0, simplify (ровно одна), WATCHDOG mem who=КЛАСС ... fsm, затем игра жива (строки журнала идут дальше).
old: тот же набор требований и он должен КРАСНЕТЬ (класса нет у машины). Код выхода 0 — всё проверено."""
import re, sys
p, c = sys.argv[1], sys.argv[2]
try: t = open(p, errors='replace').read().splitlines()
except OSError: print("нет журнала"); sys.exit(1)
tf = [l for l in t if l.startswith('TFSM %s ' % c)]
plan = [l for l in tf if ' plan ' in l]; load0 = [l for l in tf if re.search(r' load a=0 ', l)]; simp = [l for l in tf if ' simplify ' in l]
wd = [l for l in t if l.startswith('WATCHDOG mem who=%s ' % c) and ' fsm' in l]
last = max((float(m.group(1)) for l in t for m in [re.search(r' t=([\d.]+)', l)] if m), default=0)
wt = max((float(m.group(1)) for l in wd for m in [re.search(r' t=([\d.]+)', l)] if m), default=0)
ok = True
def chk(name, cond, info=''):
    global ok; print('  %-52s %s %s' % (name, 'ok' if cond else 'ПРОВАЛ', info)); ok &= bool(cond)
chk('TFSM %s plan' % c, len(plan) >= 1, len(plan))
chk('load a=0 (принудительный отказ)', len(load0) == 1, len(load0))
chk('simplify ровно один', len(simp) == 1, len(simp))
chk('WATCHDOG mem who=%s ... fsm' % c, len(wd) >= 1, len(wd))
iw = max((i for i, l in enumerate(t) if l.startswith('WATCHDOG mem who=%s ' % c)), default=-1)
# жив: сценарий дошёл до конца (EXIT739) либо, для бонус-игры (меню крутит свой цикл и время сценария там не идёт), меню бонус-игры снова на экране после отказа
alive = iw >= 0 and (any(l.startswith('EXIT739') for l in t[iw:]) or (c in ('bonus', 'menuload') and any(l.startswith('BONUS ev=menu ') for l in t[iw:])))
chk('игра жива после отказа (EXIT739 или меню бонус-игры снова открыто)', alive, '%d строк после' % (len(t) - iw - 1))
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
