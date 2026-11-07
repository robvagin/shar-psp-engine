#!/usr/bin/env python3
"""Проверка порядка загрузки по журналу прогона (ag/loadorder, №1209): игра не должна идти дальше, пока не готов пол и город рядом.
   python3 tools/loadorder_check.py smoke.txt [...] [--void-s 0.5] [--after exit|any]   -> код 1, если есть находки класса ОПАСНО
Что смотрит (строки пишет viewer/main_loop.h loFallWatch, только в служебных прогонах):
  FALL1209 who=hero|car ...   герой или машина без земли в своём мире дольше 0,35 с (комната и город перепутаны, перенос раньше открытия комнаты)  -> ОПАСНО
  VOID1209 start/end ...      видимая незагруженная клетка ближе 70 м дольше --void-s секунд (дыра рядом с игроком)                           -> ОПАСНО
      --after exit  считать только дыры, начавшиеся в 3 с после выхода из комнаты (ROOM ev=exit), --after any — любые (по умолчанию any)
Числа: у каждой находки — номер строки, сцена, ближайшая клетка в метрах, длительность в секундах.
Красный контроль: прогон со старым поведением (flag roomexitring1209old / teledoor1209old) обязан дать код 1, прогон с новым — 0 (tests/loadorder/accept.sh)."""
import re, sys

def scan(path, void_s, after):
    L = open(path, errors='replace').read().splitlines()
    scene, bad, ev_exit = 'start', [], -99.0
    starts = {}
    for i, l in enumerate(L, 1):
        if l.startswith('SCENE'):
            scene = (l.split() + ['?'])[1]
            continue
        m = re.match(r'ROOM ev=exit .* t=([\d.]+)', l)
        if m:
            ev_exit = float(m.group(1))
            continue
        if l.startswith('FALL1209'):
            bad.append((i, scene, 'ОПАСНО', l[:150]))
            continue
        m = re.match(r'VOID1209 start near=(\d+) room=(-?\d+) iris=(\d) ldon=(\d) t0=([\d.]+) t=([\d.]+)', l)
        if m:
            starts[0] = (i, scene, int(m.group(1)), float(m.group(5)), int(m.group(4)))
            continue
        m = re.match(r'VOID1209 end frames=(\d+) dur=([\d.]+) t=([\d.]+)', l)
        if m and 0 in starts:
            si, ssc, near, t0, ldon = starts.pop(0)
            dur = float(m.group(2))
            if ldon:   # экран загрузки ещё идёт — это не игра
                continue
            if dur >= void_s and (after == 'any' or 0.0 <= t0 - ev_exit <= 3.0):
                bad.append((si, ssc, 'ОПАСНО', f'дыра рядом: ближайшая незагруженная видимая клетка {near} м, {dur:.2f} с (t0={t0:.1f}; выход из комнаты t={ev_exit:.1f})'))
    return L, bad

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    void_s = 0.5; after = 'any'
    for k, a in enumerate(sys.argv):
        if a == '--void-s': void_s = float(sys.argv[k + 1]); args = [x for x in args if x != sys.argv[k + 1]]
        if a == '--after': after = sys.argv[k + 1]; args = [x for x in args if x != sys.argv[k + 1]]
    rc = 0
    for p in args:
        L, bad = scan(p, void_s, after)
        n = lambda key: sum(1 for l in L if l.startswith(key))
        print(f'== {p}: строк {len(L)}, FALL1209 {n("FALL1209")}, VOID1209 start {n("VOID1209 start")}, ROOMRING1209 {n("ROOMRING1209")}, находки {len(bad)}')
        for i, sc, sev, msg in bad[:40]:
            print(f'   {sev:8s} стр.{i} [{sc}] {msg}')
        if bad: rc = 1
    sys.exit(rc)

main()
