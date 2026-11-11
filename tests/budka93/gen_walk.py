#!/usr/bin/env python3
"""Обход входов уровня N как игрок (Будка-93, по просьбе дирижёра после 95c): каждая дверь города из interiors-lN.bin: подойти (3,7 м до двери), войти (△), 5 с внутри, выйти (в триггер выхода комнаты, △), следующая дверь; в конце будка уровня.
gen_walk.py N [DATA] > сценарий.txt   DATA — набор (по умолчанию тур-набор SHARTEST15). Лог разбирает walk_report.py"""
import math, os, struct, sys
n = int(sys.argv[1]); DATA = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/Downloads/shar-psp-t93/build/test93tour/PSP/GAME/SHARTEST15')
sub = '' if n == 1 else f'l{n}/'
d = open(f'{DATA}/{sub}interiors-l{n}.bin', 'rb').read(); _, nr, nd, ver = struct.unpack_from('<4sIII', d, 0)
rooms, off = [], 16
for i in range(nr):
    b = d[off:off + 128]; name = b[:32].split(b'\0')[0].decode(); kind = struct.unpack_from('<I', b, 64)[0]; sy = struct.unpack_from('<f', b, 36)[0]
    ctr = struct.unpack_from('<3f', b, 68) if kind != 0xFFFFFFFF else None
    sx, sz = struct.unpack_from('<f', b, 32)[0], struct.unpack_from('<f', b, 40)[0]
    rooms.append((name, ctr, sy, sx, sz)); off += 128
doors = []
for i in range(nd):
    b = d[off:off + 128]; x, y, z = struct.unpack_from('<3f', b, 32); idx, = struct.unpack_from('<i', b, 44); h, = struct.unpack_from('<f', b, 48); doors.append((idx, x, y, z, h)); off += 128
BOOTH = {1: (30.6, 205.1), 7: (30.6, 205.1), 2: (-8.8, 3.6), 5: (-8.8, 3.6), 3: (91.0, 140.0), 6: (91.0, 140.0), 4: (144.0, 81.8)}
o = ['scene C_BOOT', 'flag cfgdefaults 1', 'wait 6']
if n > 1: o += [f'scene L{n}_load', f'flag golevel {n}', 'wait 45']
else: o += ['scene L1_load', 'wait 8']
o += ['flag player', 'wait 2']
for k, (idx, x, y, z, h) in enumerate(doors):
    rname = rooms[idx][0]; hin = h + math.pi; hin = math.atan2(math.sin(hin), math.cos(hin))
    fx, fz = x - 2.4 * math.sin(hin), z - 2.4 * math.cos(hin)   # 2,4 м до двери: дальше дом упирался в крыльцо
    o += [f'scene D{k}_{rname}_in', f'foot {fx:.2f} {fz:.2f} {hin:.4f} {y + 0.4:.2f}', 'wait 1.5', 'press up 0.6', 'wait 0.4', 'press triangle 0.3', 'wait 1', 'press triangle 0.3', 'wait 5', f'shot in{k}']
    ex = rooms[idx][1]
    if ex: o += [f'scene D{k}_{rname}_out', f'foot {ex[0]:.2f} {ex[2]:.2f} 0 {ex[1]:.2f}', 'wait 0.6', 'press triangle 0.3', 'wait 1', 'press triangle 0.3', 'wait 5', f'shot out{k}']   # y — центр триггера выхода
    else: o += [f'scene D{k}_{rname}_noexit', 'wait 1']
bx, bz = BOOTH[n]
o += [f'scene BOOTH{n}', f'foot {bx + 0.5} {bz + 1.2} 0', 'wait 3', 'press triangle 0.2', 'wait 10', f'shot booth', 'press cross 0.2', 'wait 12', 'shot booth_after', 'end']
print('\n'.join(o))
