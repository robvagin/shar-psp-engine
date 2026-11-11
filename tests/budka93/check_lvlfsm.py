#!/usr/bin/env python3
"""Будка-93 / Build 96 (Fable §6 шаг 10): LEVEL_NEXT на ldrRun. check_lvlfsm.py smoke.txt ok|f2|f5|old
ok: TFSM levelup plan, close, free, load a=1, ready, open по одной, ровно один LOADNEWS и один XITION level на переход, уровень загружен, WATCHDOG mem 0.
f2 (tffail 2): WATCHDOG mem who=levelup step=1 и step=2, затем load a=1, один LOADNEWS. f5 (tffail 5): ступени 1..4, load a=0, SIMPLIFY (step=5), уровень всё равно загружен, без ready/open. old (flag lvldr96 0): красный — строки free нет."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
s = next((i for i, l in enumerate(L) if l.startswith('LEVELUP')), -1)
T = L[s:] if s >= 0 else []
def cnt(p): return sum(1 for l in T if re.search(p, l))
ok = True
def chk(n, v, i=''):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n + (' ' + str(i) if i != '' else ''))
chk('LEVELUP наступил', s >= 0)
for ph in (('plan', 'close', 'free') if mode == 'f5' else ('plan', 'close', 'free', 'ready', 'open')): chk('TFSM levelup %s ровно одна' % ph, cnt(r'^TFSM levelup %s ' % ph) == 1, cnt(r'^TFSM levelup %s ' % ph))
chk('TFSM levelup load a=%d ровно одна' % (0 if mode == 'f5' else 1), cnt(r'^TFSM levelup load a=%d ' % (0 if mode == 'f5' else 1)) == 1)
chk('один LOADNEWS на переход', cnt(r'^LOADNEWS ') == 1, cnt(r'^LOADNEWS '))
chk('один XITION level', cnt(r'^XITION level') == 1, cnt(r'^XITION level'))
chk('уровень загружен (LEVEL ev=load n=4)', cnt(r'^LEVEL ev=load n=4 ') >= 1)
wd = [l for l in T if l.startswith('WATCHDOG mem who=levelup')]
if mode == 'ok': chk('WATCHDOG mem who=levelup 0', not wd, len(wd))
if mode == 'f2': chk('ступени 1 и 2 (WATCHDOG step=1, step=2)', any('step=1 ' in l for l in wd) and any('step=2 ' in l for l in wd) and not any('step=5' in l for l in wd), len(wd))
if mode == 'f5': chk('SIMPLIFY (step=5) и уровень загружен', any('step=5 simplify' in l for l in wd))
chk('END', any(l == 'END' for l in L))
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
