#!/usr/bin/env python3
"""№1166: сторож данных камеры в наборе (tools/ship.sh, после сборки данных). Закон объектива и камеры зон считают по данным уровня: коллизия COL1 внутри level.shr
(стены, заборы, коробки, цилиндры), камеры зон города (zonecam-l<N>.shr), камеры машины у зон прыжка (carzone-l<N>.shr), камеры старта миссий (msncam.shr),
камеры-вырезы (camcut-l<N>.shr), комнаты (interiors-l<N>.bin, -gfx.bin). Нет любого из них или COL1 пуста — камера молча теряет правило (движок читает «нет файла» как «камер нет»),
и у собравшего порт она идёт сквозь стены. Проверка: файлы на месте и не пусты, COL1 по заголовку SHR7 @92 читается и в ней есть треугольники, заборы и ячейки.
    python tools/camera_data_gate.py НАБОР [--levels 1,3]   (по умолчанию: уровень 1 и все папки l<N>/ набора); код выхода 0 — всё есть, последняя строка — итог
Выключатель для ship.sh: CAMDATA1166_OLD=1 — без проверки."""
import os, struct, sys
def col_ok(path):
    sz = os.path.getsize(path)
    with open(path, 'rb') as f: head = f.read(128); off = struct.unpack_from('<I', head, 92)[0]
    if off <= 0 or off + 64 > sz: return 'смещение COL1 в заголовке (@92) вне файла'
    with open(path, 'rb') as f: f.seek(off); h = f.read(64)
    if h[:4] != b'COL1': return 'по смещению @92 нет COL1'
    W, H, nt, nb, nc, ns, nr = struct.unpack_from('<IIIIIII', h, 16)
    if not (W > 0 and H > 0 and nt > 0 and ns > 0 and nr > 0): return 'COL1 пуста (W=%d H=%d треугольников %d заборов %d ссылок %d)' % (W, H, nt, ns, nr)
    need = 48 + nt * 48 + nb * 64 + nc * 48 + ns * 32 + (W * H + 1) * 4 + nr * 4
    if off + need > sz: return 'COL1 длиннее файла'
    return None
def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]; stage = a[0]
    lv = None
    if '--levels' in sys.argv: lv = [int(x) for x in sys.argv[sys.argv.index('--levels') + 1].split(',')]
    if lv is None: lv = [1] + sorted(int(d[1:]) for d in os.listdir(stage) if len(d) == 2 and d[0] == 'l' and d[1].isdigit() and os.path.isdir(os.path.join(stage, d)))
    bad = []; n = 0
    for L in lv:
        base = stage if L == 1 else os.path.join(stage, 'l%d' % L)
        for name in ('level.shr', 'zonecam-l%d.shr' % L, 'carzone-l%d.shr' % L, 'msncam.shr', 'camcut-l%d.shr' % L, 'interiors-l%d.bin' % L, 'interiors-l%d-gfx.bin' % L):
            p = os.path.join(base, name); n += 1
            if not os.path.exists(p): bad.append('L%d: нет %s' % (L, name)); continue
            if os.path.getsize(p) == 0 and not (name.startswith('camcut') and L >= 4):   # у уровней 4–7 камер-вырезов на диске нет: файл пустой (camcut_export.py)
                bad.append('L%d: %s пуст' % (L, name)); continue
            if name == 'level.shr':
                e = col_ok(p)
                if e: bad.append('L%d: level.shr: %s' % (L, e))
    for b in bad: print('ПРОВАЛ', b)
    print('данные камеры: %s (уровни %s, файлов %d)' % ('ВСЁ НА МЕСТЕ' if not bad else 'ПРОВАЛ: ' + '; '.join(bad), ','.join(map(str, lv)), n))
    sys.exit(1 if bad else 0)
main()
