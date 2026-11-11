#!/usr/bin/env python3
"""П3 пропечка, шаг 11 (Fable-спецификация 08.11, В.4): порядок блобов клеток в level.shr по маршрутам миссий.
python3 tools/cell_order.py LEVEL.shr OUT.shr [--interiors interiors-lN.bin] [--msncam msncam.shr] [--points файл] [--radius 270] [--report]

Сегодня блобы клеток лежат по строкам карты; круг у точки (старт, переноса миссии, двери) состоит из клеток, разбросанных по файлу, и читается десятками чтений (14–30).
Здесь для каждой точки маршрута клетки круга радиуса R (по умолчанию 270 м = DIST_LONG+10 круга старта в движке, по центрам клеток) сортируются по расстоянию и кладутся подряд, группа за группой, в порядке маршрута:
  1) старт уровня (заголовок SHR7, spawn); 2) двери города (SHI1 interiors-lN.bin, p[3]); 3) камеры фаз миссий по номеру (msncam.shr, MCM1: глаз последнего кадра — рядом с местом, куда ставят игрока);
  4) точки из --points (строки «x z», в порядке файла); оставшиеся клетки — в прежнем порядке после групп.
Клетка в нескольких кругах остаётся в первой группе; в остальных её нет рядом, стык через RING_GAP (64 КБ) читается одним чтением, если разрыв невелик.
Меняется только порядок блобов в хвосте файла и смещение (@36, 48 Б записи) в клеточной таблице: размеры, содержимое блобов, остальные секции, заголовок не трогаются.
Проверка: tests/load2/order_check.py OLD NEW (те же байты каждой клетки и секций; --plant — красный контроль) и tools/cell_order.py ... --report (чтений круга до/после по моделированию cellsReadRuns: стык ≤ 64 КБ, отрезок ≤ 512 КБ)."""
import struct, sys, math
from pathlib import Path

def parse(d):
    assert d[:4] == b'SHR7'
    toff, ncell, coff = struct.unpack_from('<III', d, 76)
    hd = struct.unpack_from('<14f', d, 12)
    cells = []
    for i in range(ncell):
        r = d[coff + 48 * i:coff + 48 * i + 48]
        bb = struct.unpack_from('<6f', r, 0); first, n, owner, off, size, pad = struct.unpack_from('<6I', r, 24)
        cells.append(dict(i=i, bb=bb, cx=(bb[0] + bb[3]) / 2, cz=(bb[2] + bb[5]) / 2, first=first, n=n, owner=owner, off=off, size=size))
    return hd, coff, cells

def door_points(path):
    b = Path(path).read_bytes()
    if b[:4] != b'SHI1': return []
    nroom, ndoor, ver = struct.unpack_from('<III', b, 4)
    if ver < 2: return []
    pos = 16 + 128 * nroom; pts = []
    for k in range(ndoor):
        x, y, z = struct.unpack_from('<3f', b, pos + 128 * k + 32); pts.append((x, z))
    return pts

def msncam_points(path):
    b = Path(path).read_bytes()
    if b[:4] != b'MCM1': return []
    n = struct.unpack_from('<I', b, 4)[0]; rows = []
    for i in range(n):
        r = b[16 + 64 * i:16 + 64 * i + 64]
        mission = struct.unpack_from('<i', r, 32)[0]; nfr = struct.unpack_from('<I', r, 36)[0]; off = struct.unpack_from('<I', r, 44)[0]
        if not nfr or off + 40 * nfr > len(b): continue
        lx, ly, lz = struct.unpack_from('<3f', b, off + 40 * (nfr - 1))   # кадр: pos3 look3 up3 fov (look — направление); глаз последнего кадра стоит у игрока
        rows.append((mission, lx, lz))
    rows.sort(); return [(x, z) for m, x, z in rows]

def ring_reads(cells, pts_xz, R=270.0, gap=64 << 10, runmax=512 << 10):
    """сколько чтений нужно кругу у точки: клетки круга по возрастанию смещения, соседние сливаются при разрыве ≤ gap и отрезке ≤ runmax (как cellsReadRuns)"""
    out = []
    for x, z in pts_xz:
        ids = sorted((c['off'], c['size']) for c in cells if not c['owner'] and math.hypot(c['cx'] - x, c['cz'] - z) < R)
        reads = 0; a = 0
        while a < len(ids):
            s = ids[a][0]; e = s + ids[a][1]; b = a + 1
            while b < len(ids) and ids[b][0] >= e and ids[b][0] - e <= gap and ids[b][0] + ids[b][1] - s <= runmax: e = ids[b][0] + ids[b][1]; b += 1
            reads += 1; a = b
        out.append((len(ids), reads))
    return out

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    opt = {}
    args = sys.argv[1:]
    for k in ('--interiors', '--msncam', '--points', '--radius'):
        if k in args: opt[k] = args[args.index(k) + 1]
    src, out = Path(a[0]), Path(a[1])
    d = bytearray(src.read_bytes()); hd, coff, cells = parse(d)
    R = float(opt.get('--radius', 270))
    pts = [(hd[6], hd[8])]                      # старт уровня: spawn x, z
    if '--interiors' in opt: pts += door_points(opt['--interiors'])
    if '--msncam' in opt: pts += msncam_points(opt['--msncam'])
    if '--points' in opt:
        for l in Path(opt['--points']).read_text().splitlines():
            t = l.split()
            if len(t) >= 2 and not l.startswith('#'): pts.append((float(t[0]), float(t[1])))
    placed = set(); order = []
    for x, z in pts:
        grp = sorted((math.hypot(c['cx'] - x, c['cz'] - z), c['i']) for c in cells if not c['owner'] and c['size'] and math.hypot(c['cx'] - x, c['cz'] - z) < R and c['i'] not in placed)
        for _, i in grp: placed.add(i); order.append(i)
    rest = sorted((c for c in cells if c['size'] and c['i'] not in placed), key=lambda c: c['off']); order += [c['i'] for c in rest]
    offs = [c['off'] for c in cells if c['size']]; lo = min(offs); hi = max(c['off'] + c['size'] for c in cells if c['size'])
    align = 16 if all(o % 16 == 0 for o in offs) else 4 if all(o % 4 == 0 for o in offs) else 1
    # прежняя раскладка: сохраняем границы региона блобов и выравнивание; зазоры между блобами в новом порядке — только выравнивание
    blobs = {c['i']: bytes(d[c['off']:c['off'] + c['size']]) for c in cells if c['size']}
    pos = lo; new_off = {}
    region = bytearray()
    for i in order:
        pos = (pos + align - 1) // align * align; new_off[i] = pos
        region += b'\0' * (pos - lo - len(region)); region += blobs[i]; pos = lo + len(region)
    tail_gap = hi - (lo + len(region))
    if tail_gap < 0: raise SystemExit('cell_order: новый регион длиннее прежнего (%d > %d) — выравнивание' % (len(region), hi - lo))
    region += b'\0' * tail_gap
    d[lo:hi] = region
    for c in cells:
        if c['size']: struct.pack_into('<I', d, coff + 48 * c['i'] + 36, new_off[c['i']])
    out.write_bytes(d)
    new_cells = [dict(c, off=new_off.get(c['i'], c['off'])) for c in cells]
    print(f'cell_order: {src.name}: точек маршрута {len(pts)}, клеток {len(cells)} (в группах {len(placed)}), регион {lo}..{hi} ({(hi - lo) >> 10} КБ), выравнивание {align}')
    if '--report' in args:
        before = ring_reads(cells, pts); after = ring_reads(new_cells, pts)
        print('  чтений круга 270 м у точек маршрута (клеток / чтений): до -> после')
        for k, ((n0, r0), (n1, r1)) in enumerate(zip(before, after)):
            print(f'  точка {k} ({pts[k][0]:.0f},{pts[k][1]:.0f}): {n0} кл. {r0} -> {r1}')
        print(f'  среднее чтений: {sum(r for n, r in before) / len(before):.1f} -> {sum(r for n, r in after) / len(after):.1f}; максимум {max(r for n, r in before)} -> {max(r for n, r in after)}')

if __name__ == '__main__': main()
