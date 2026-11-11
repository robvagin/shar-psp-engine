#!/usr/bin/env python3
"""Будка-93 / Build 96: проверка конца уровня N с роликом на пути levelup. check_fmvend.py smoke.txt N FMV
Цепочка: MSNFMV name=fmvK played=1 -> MSN level-complete level=N -> LEVELUP N->N+1 -> TFSM levelup plan..open по одной -> один LOADNEWS -> LEVEL ev=load n=N+1; WATCHDOG mem 0, MEMFAILSZ 0 за переход; память ролика (журнал movie/MOVIE и ZONE3 place) печатается."""
import sys, re
f, N, F = sys.argv[1], int(sys.argv[2]), sys.argv[3]
L = open(f, errors='replace').read().split('\n')
kv = lambda l: dict(p.split('=', 1) for p in l.split()[1:] if '=' in p)
ok = True
def chk(n, v, i=''):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n + (' ' + str(i) if i != '' else ''))
fm = [(i, kv(l)) for i, l in enumerate(L) if l.startswith('MSNFMV ')]
chk('ролик fmv%s сыгран целиком' % F, fm and fm[0][1].get('name') == 'fmv' + F and fm[0][1].get('played') == '1', fm[0][1] if fm else 'нет MSNFMV')
ic = next((i for i, l in enumerate(L) if l.startswith('MSN level-complete level=%d' % N)), -1); chk('Level Complete (SetLevelOver)', ic >= 0 and (not fm or ic > fm[0][0]))
iu = next((i for i, l in enumerate(L) if re.match(r'LEVELUP from=%d to=%d' % (N, N + 1), l)), -1); chk('LEVELUP %d->%d после Level Complete' % (N, N + 1), iu > ic >= 0)
T = L[iu:] if iu >= 0 else []
c = lambda p: sum(1 for l in T if re.search(p, l))
for ph in ('plan', 'close', 'free', 'ready', 'open'): chk('TFSM levelup %s по одной' % ph, c(r'^TFSM levelup %s ' % ph) == 1, c(r'^TFSM levelup %s ' % ph))
chk('одна газета (LOADNEWS)', c(r'^LOADNEWS ') == 1, c(r'^LOADNEWS '))
chk('уровень %d загружен' % (N + 1), c(r'^LEVEL ev=load n=%d ' % (N + 1)) >= 1)
chk('WATCHDOG mem who=levelup 0', c(r'^WATCHDOG mem who=levelup') == 0)
pre = L[:iu] if iu >= 0 else L
mf = [l for l in L if l.startswith('MEMFAILSZ')]; chk('MEMFAILSZ 0 за весь прогон', not mf, len(mf))
wd = [l for l in L if l.startswith('WATCHDOG mem') and ' fsm' not in l]; chk('WATCHDOG mem (ролик, лестница) 0', not wd, len(wd))
chk('END', any(l == 'END' for l in L))
for l in L:
    if re.match(r'(MSNFMV|MOVIEMEM|MOVIE |MOVIEOPEN)', l): print('   ' + l[:170])
z = [l for l in L if l.startswith('ZONE3 ') and ' place ' in l]
if z: print('   ZONE3 последняя до LEVELUP:', [l for l in L[:iu] if l.startswith('ZONE3 ')][-1][:210] if iu > 0 and any(l.startswith('ZONE3 ') for l in L[:iu]) else '-')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
