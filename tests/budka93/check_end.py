#!/usr/bin/env python3
"""Будка-93 / 1234: проверка конца уровня N по smoke.txt. check_end.py smoke.txt N
Цепочка по порядку: MSN complete -> LEVELDONE n=N code=1 -> LVSAVEASK open -> closed -> LEVELUP N->N+1 -> LEVEL ev=load n=N+1 -> XITION level; потом ни STALL, ни GE NESTED, ни WATCHDOG (кроме spawn hero и mem). Печатает, где цепочка встала."""
import sys, re
f, N = sys.argv[1], int(sys.argv[2])
L = open(f, errors='replace').read().split('\n')
steps = [('MSN complete', r'^MSN complete'), ('LEVELDONE', rf'^LEVELDONE n={N} code=1'), ('LVSAVEASK open', r'^LVSAVEASK open'), ('LVSAVEASK closed', r'^LVSAVEASK closed'),
         ('LEVELUP', rf'^LEVELUP from={N} to={N+1}'), ('LEVEL ev=load', rf'^LEVEL ev=load n={N+1} '), ('XITION level', r'^XITION level')]
if N == 7: steps = [('MSN complete', r'^MSN complete'), ('LEVELDONE', r'^LEVELDONE n=7 code=2'), ('ENDSAVE open', r'^ENDSAVE open')]
pos = 0; ok = True
for name, pat in steps:
    i = next((k for k in range(pos, len(L)) if re.search(pat, L[k])), -1)
    print(('PASS ' if i >= 0 else 'FAIL ') + name + (f' (строка {i+1})' if i >= 0 else ' НЕ НАСТУПИЛО: цепочка встала здесь'))
    if i < 0: ok = False; break
    pos = i
if N < 7:   # Build 96: LEVEL_NEXT на ldrRun — на естественный переход ровно одна газета и одна машина переходов
    j = next((k for k, l in enumerate(L) if re.search(rf'^LEVELUP from={N} to={N+1}', l)), -1); T2 = L[j:] if j >= 0 else []
    nn = sum(1 for l in T2 if l.startswith('LOADNEWS ')); fs = sum(1 for l in T2 if re.match(r'TFSM levelup (free|open) ', l))
    print(('PASS ' if nn == 1 else 'FAIL ') + 'один LOADNEWS на переход: %d' % nn); print(('PASS ' if fs == 2 else 'FAIL ') + 'TFSM levelup free+open: %d' % fs); ok &= nn == 1 and fs == 2
bad = [l for l in L[pos:] if re.search(r'STALL|GE NESTED|^MEMFAILSZ|ROOM ev=fail', l) or (l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l))]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
print(('PASS ' if not bad else 'FAIL ') + 'после перехода без STALL/NESTED/MEMFAILSZ/WATCHDOG: %d' % len(bad))
for l in bad[:5]: print('   ' + l[:140])
ok &= not bad; ok &= any(l == 'END' for l in L)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
