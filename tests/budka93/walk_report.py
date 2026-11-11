#!/usr/bin/env python3
"""Сводка обхода входов (gen_walk.py): walk_report.py LEVEL smoke.txt  — по каждой двери: вход, выход, лишний экран, наезд камеры, WATCHDOG, MEMFAILSZ, отказ комнаты; и по будке.
Проблемы: вход отменён молча (нет ROOM ev=enter и нет ROOM ev=fail), второй экран (LOADNEWS внутри входа/выхода), наезд (MSNCAM start), WATCHDOG (кроме spawn hero, mem-ступеней, zone3 shrink/regrow, cellthrash), MEMFAILSZ, ROOM ev=fail."""
import sys, re
lvl, f = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
segs = []; cur = None
for l in L:
    m = re.match(r'^SCENE (\S+)', l)
    if m: cur = [m.group(1), []]; segs.append(cur); continue
    if cur: cur[1].append(l)
def cnt(lines, p): return sum(1 for l in lines if re.search(p, l))
def bad(lines): return [l for l in lines if (l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l))]
rows = []; probs = []
for name, lines in segs:
    m = re.match(r'D(\d+)_(\w+?)_(in|out|noexit)$', name)
    if m:
        k, room, kind = m.groups(); enter = cnt(lines, r'^ROOM ev=enter'); ex = cnt(lines, r'^ROOM ev=exit'); fail = cnt(lines, r'^ROOM ev=fail')
        news = cnt(lines, r'^LOADNEWS'); cam = cnt(lines, r'^MSNCAM start'); mf = cnt(lines, r'^MEMFAILSZ'); wd = len(bad(lines)); tf = cnt(lines, r'^TFSM room_in plan a=\d+ b=2')
        ok = (kind == 'in' and enter == 1 and not fail) or (kind == 'out' and ex == 1) or kind == 'noexit'
        p = []
        if not ok: p.append('нет входа/выхода' + (' (отказ комнаты)' if fail else ' (молча)'))
        if news: p.append('второй экран ×%d' % news)
        if cam: p.append('наезд камеры')
        if wd: p.append('WATCHDOG ×%d' % wd)
        if mf: p.append('MEMFAILSZ ×%d' % mf)
        rows.append((f'{room} {kind}', 'ok' if ok else 'НЕТ', 'прелоадер' if tf else '', '; '.join(p) or 'чисто'))
        probs += p
    elif name.startswith('BOOTH'):
        opened = cnt(lines, r'^PHONE view open'); fail = cnt(lines, r'PHONE open-fail'); news = cnt(lines, r'^LOADNEWS'); cam = cnt(lines, r'^MSNCAM start'); wd = len(bad(lines)); mf = cnt(lines, r'^MEMFAILSZ')
        p = []
        if not opened: p.append('будка не открылась' + (' (open-fail)' if fail else ' (молча)'))
        if news > 1: p.append('экранов загрузки ×%d' % news)
        if cam: p.append('наезд камеры')
        if wd: p.append('WATCHDOG ×%d' % wd)
        if mf: p.append('MEMFAILSZ ×%d' % mf)
        rows.append(('будка', 'ok' if opened else 'НЕТ', '', '; '.join(p) or 'чисто')); probs += p
print(f'L{lvl}: END={"да" if any(l == "END" for l in L) else "НЕТ (не дошёл)"}')
for r in rows: print('  %-26s %-4s %-10s %s' % r)
print('  итог: ' + ('чисто' if not probs and any(l == 'END' for l in L) else 'ПРОБЛЕМЫ: ' + '; '.join(sorted(set(probs)))))
