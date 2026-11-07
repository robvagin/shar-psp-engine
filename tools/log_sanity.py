#!/usr/bin/env python3
"""Санитарный разбор журнала тура (smoke.txt с эмулятора или с карты PSP) — черновик гейта (arch-fable, 05.11.2026).
   python3 ЧИСТОТА_TEST90_log_sanity.py smoke.txt [...]   -> код 1, если есть находки класса ОПАСНО
Ловит то, чего frame_gate/тур не смотрят: FOOT в координаты комнаты без комнаты у героя (№1209), ROOM ev=fail, резкое падение
героя (> 4 м за ≤ 1,5 с без телепорта/старта миссии), MEMFAILSZ/STALL/GE NESTED/WAITLIMIT/ME HOLD miss/LVCHECK fail/CRASH,
ролик не сыгран (MSNFMV played=0). Высота «под миром» — только относительно пола коллизии (L3/L6 стоят на −64…−68 м), поэтому
абсолютного порога нет: падение = просадка между соседними отсчётами HOMER."""
import sys, re, collections
RESET = ('TELEPORT', 'FOOT', 'GAME ', 'LEVELUP', 'MSN ', 'MSNEWS', 'MSNCAM', 'ROOM ev=', 'MSNRESET', 'RESET')
ALARM = ('MEMFAILSZ', 'STALL', 'GE NESTED', 'WAITLIMIT', 'ME HOLD miss', 'LVCHECK fail', 'CRASH', 'ROOM ev=fail', 'ROOMDL ev=fail', 'COLL error', 'error=memory', 'FALL1209')   # FALL1209 — наблюдатель падения движка (ag/loadorder): герой/машина без земли в своём мире > 0,35 с
rc = 0
for path in sys.argv[1:]:
    L = open(path, errors='replace').read().splitlines()
    scene, room, prev, bad, stats = 'start', '-', None, [], collections.Counter()
    for i, l in enumerate(L, 1):
        if l.startswith('SCENE'): scene = (l.split() + ['?'])[1]
        m = re.match(r'HOMER t=([\d.]+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) h=\S+ room=(\S+)', l)
        if m:
            t, y, room = float(m.group(1)), float(m.group(3)), m.group(5)
            if prev and t - prev[0] <= 1.5 and prev[1] - y > 4.0:
                bad.append((i, scene, 'ОПАСНО', f'падение героя y {prev[1]:.1f} -> {y:.1f} за {t - prev[0]:.1f} с')); stats['падение'] += 1
            prev = (t, y); continue
        if l.startswith(RESET): prev = None
        m = re.match(r'FOOT (\S+) (\S+) (\S+) \S+ t=', l)
        # координаты комнат порта: все интерьеры лежат в одном кармане x 480…520, z 280…470, пол y = −20 (interiors-l1/l4.bin);
        # низины L3/L6 (−64…−77 м) — это земля города, не комнаты
        inroombox = m and 480 <= float(m.group(1)) <= 520 and 280 <= float(m.group(3)) <= 470 and -22 <= float(m.group(2)) <= -18
        if inroombox and room in ('-', '?'):
            bad.append((i, scene, 'ОПАСНО', f'FOOT в координаты комнаты (y={float(m.group(2)):.1f}), герой не в комнате (room={room}) — класс №1209')); stats['foot_без_комнаты'] += 1
        for k in ALARM:
            if k in l: bad.append((i, scene, 'ОПАСНО' if k in ('STALL', 'GE NESTED', 'CRASH', 'ROOM ev=fail', 'FALL1209') else 'проверить', l[:110])); stats[k] += 1
        m = re.match(r'MSNFMV name=(\S+).*?played=(\d)', l)
        if m and m.group(2) == '0': bad.append((i, scene, 'проверить', l[:100])); stats['ролик_не_сыгран'] += 1
    print(f"== {path}: строк {len(L)}, сцен {sum(1 for l in L if l.startswith('SCENE'))}, находки {dict(stats) or 'нет'}")
    for i, sc, sev, msg in bad[:60]: print(f"   {sev:9s} стр.{i} [{sc}] {msg}")
    if any(b[2] == 'ОПАСНО' for b in bad): rc = 1
sys.exit(rc)
