#!/usr/bin/env python3
"""Шаг 8 (Fable-спецификация 08.11, Z2): расчёт плит пула клеток для области города — что даёт набор классов 16/32/64/128/256 КБ против нынешнего 16/32/64/128/192.
python3 tools/z2_plan.py НАБОР_С_УРОВНЯМИ [--radius 150,220,270]

Данные: таблицы клеток level.shr всех семи уровней (размер блоба каждой клетки). Рабочий набор — клетки в круге радиуса r вокруг 30 равномерно взятых точек уровня (центр мира клеток),
r = дальность+20 м при 200 м (220), круг старта (270) и ближний (150). Для каждого набора классов: клетка округляется вверх до класса (клетки крупнее старшего класса идут отдельным блоком, как cpAllocCell),
ячейки класса укладываются в плиты по cpSlabBytes (≥ 4 ячеек, не больше 32, кратно 256 КБ), плита занимает цельное место; считаем байты плит (сколько занимает область города) и долю потерь.
Это модель по таблицам клеток, не журнал железа: хвосты клеток (cpAlloc, куски по 256 КБ) не считаются."""
import sys, math, struct, random
from pathlib import Path

def slab_bytes(cell):
    b = 256 << 10
    while b // cell < 4: b += 256 << 10
    if b // cell > 32: b = 32 * cell
    return b

def need(sizes, classes_kb):
    cls = [k << 10 for k in classes_kb]; n = {c: 0 for c in cls}; big = 0; req = 0
    for s in sizes:
        req += s
        for c in cls:
            if c >= s: n[c] += 1; break
        else: big += s
    area = big; cells_waste = 0
    for c in cls:
        if not n[c]: continue
        per = slab_bytes(c) // c; slabs = -(-n[c] // per); area += slabs * slab_bytes(c)
    return area, req, big

def level_cells(path):
    d = Path(path).read_bytes(); toff, ncell, coff = struct.unpack_from('<III', d, 76); out = []
    for i in range(ncell):
        r = d[coff + 48 * i:coff + 48 * i + 48]; bb = struct.unpack_from('<6f', r, 0); owner, off, size = struct.unpack_from('<III', r, 32)
        if size and not owner: out.append(((bb[0] + bb[3]) / 2, (bb[2] + bb[5]) / 2, size))
    return out

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]; S = Path(a[0])
    radii = [float(x) for x in (sys.argv[sys.argv.index('--radius') + 1] if '--radius' in sys.argv else '150,220,270').split(',')]
    sets = {'нынешние 16/32/64/128/192': [16, 32, 64, 128, 192], 'плиты 16/32/64/128/256': [16, 32, 64, 128, 256], 'мельче 8/12/16/24/32/48/64/96/128/192': [8, 12, 16, 24, 32, 48, 64, 96, 128, 192],
            'мельче и 256 8/16/24/32/48/64/96/128/192/256': [8, 16, 24, 32, 48, 64, 96, 128, 192, 256]}
    random.seed(7)
    print(f'{"уровень":<7}{"r, м":>6}{"клеток":>8}{"блобы МБ":>10}  ' + '  '.join(f'{k.split()[0] + " " + k.split()[1][:14]:>26}' for k in sets))
    for lv in range(1, 8):
        p = S / ('level.shr' if lv == 1 else f'l{lv}/level.shr')
        if not p.is_file(): continue
        cells = level_cells(p); xs = [c[0] for c in cells]; zs = [c[1] for c in cells]
        for r in radii:
            rows = {k: [] for k in sets}; ncs = []; reqs = []
            for _ in range(30):
                cx = random.choice(cells); x, z = cx[0], cx[1]
                ws = [c[2] for c in cells if math.hypot(c[0] - x, c[1] - z) < r]
                if len(ws) < 5: continue
                ncs.append(len(ws)); reqs.append(sum(ws))
                for k, cl in sets.items(): rows[k].append(need(ws, cl)[0])
            if not ncs: continue
            big = sum(1 for c in cells if c[2] > (192 << 10)); big2 = sum(1 for c in cells if c[2] > (256 << 10))
            print(f'L{lv:<6}{r:>6.0f}{sum(ncs) / len(ncs):>8.0f}{sum(reqs) / len(reqs) / 1048576:>10.2f}  ' + '  '.join(f'{sum(v) / len(v) / 1048576:>8.2f} (p95 {sorted(v)[int(len(v) * .95)] / 1048576:>5.2f}, +{100 * (sum(v) / len(v) / (sum(reqs) / len(reqs)) - 1):>3.0f}%)' for v in rows.values()))
        print(f'        клеток крупнее 192 КБ: {big}, крупнее 256 КБ: {big2} (из {len(cells)})')
if __name__ == '__main__': main()
