#!/usr/bin/env python3
"""Перепись правки 1293 (Бёрнс в L1 m7 не виден до разговора): точки постановки персонажей миссий L1–L7 и клетки города, накрывающие точку по X/Z.
Персонаж миссии не рисуется (канон 11), пока ВСЕ накрывающие клетки в памяти; клетки вне набора районов этапа загрузчик не держит, клетки выше/ниже ног — чужой пол.
  python3 -I tools/npc_scene_census.py [--shr-dir ПАПКА_НАБОРА] [--root ДЕРЕВО_С_ДАННЫМИ]   (ДЕРЕВО_С_ДАННЫМИ — где donut/ и hit-and-run-web/, по умолчанию ~/Downloads/shar-psp)
Строка на (уровень, миссия, этап, персонаж): точка, набор районов этапа, накрывающие клетки (номер район высота), «чужие» (район не в наборе) и «по высоте» (ноги ±3 м вне диапазона клетки);
ИТОГ: сколько точек, у которых старое правило не дождётся (есть чужая клетка), и сколько из них новое правило закрывает."""
import sys, json, struct, re, os, subprocess, argparse, tempfile
from pathlib import Path
ap = argparse.ArgumentParser()
ap.add_argument('--shr-dir', default=str(Path.home() / 'Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15'))
ap.add_argument('--root', default=str(Path.home() / 'Downloads/shar-psp'))
a = ap.parse_args(); ROOT = Path(a.root); SHR = Path(a.shr_dir)
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/missions')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import regions_export as RX
tmp = tempfile.mkdtemp()
subprocess.run([sys.executable, str(ROOT / 'tools/missions/compile.py'), '--all', '--out', tmp + '/out', '--scratch', tmp, '-q'], cwd=str(ROOT), check=True, stdout=subprocess.DEVNULL)
DY = 3.0
tot = bad_old = fixed = risky = 0; rows = []
for L in range(1, 8):
    d = json.load(open(f'{tmp}/level0{L}.json')); locs = {l['name']: l for l in d['locators']}
    f = SHR / ('level.shr' if L == 1 else f'l{L}/level.shr'); b = f.read_bytes()
    hw, hh = struct.unpack_from('ii', b, 12 + 14 * 4); ncell, cellOff = struct.unpack_from('<II', b, 80)
    cells = []
    for i in range(ncell):
        o = cellOff + 48 * i; bb = struct.unpack_from('<6f', b, o); first, n, owner, off, size, pad = struct.unpack_from('<6I', b, o + 24)
        if owner == 0: cells.append((i, bb, pad & 0xFF))
    names = RX.level_regions(L)
    def mask(files):
        m = 0
        for s in files.replace(':', ';').split(';'):
            s = s.strip().lower()
            if s.endswith('.p3d') and s[:-4] in [n.lower() for n in names]: m |= 1 << [n.lower() for n in names].index(s[:-4])
        return m
    pts = []   # (миссия, фаза, этап, имя, pos, набор)
    for m in d['missions']:
        for ph in m['phases']:
            S = 0
            for c in ph.get('setup', []) + ph.get('load', []):
                if c['op'] == 'SetDynaLoadData' and c['args'] and isinstance(c['args'][0], str): S |= mask(c['args'][0])
            for si, st in enumerate(ph['stages']):
                for c in st.get('cmds', []):
                    if c['op'] in ('AddNPC', 'AddStageCharacter') and len(c['args']) > 1 and isinstance(c['args'][1], dict) and 'pos' in c['args'][1]:
                        pts.append((m['id'], ph['name'], si + 1, str(c['args'][0]), c['args'][1]['pos'], S))
    for mid, ph, si, nm, p, S in pts:
        x, y, z = p; cov = [(i, bb, r) for i, bb, r in cells if bb[0] <= x <= bb[3] and bb[2] <= z <= bb[5]]
        foreign = [(i, r) for i, bb, r in cov if r and r <= 16 and S and not (S >> (r - 1)) & 1]
        high = [(i, round(bb[1], 1), round(bb[4], 1)) for i, bb, r in cov if y + DY < bb[1] or y - DY > bb[4]]
        tot += 1
        if foreign:
            bad_old += 1
            if any(not (y + DY < bb[1] or y - DY > bb[4]) for i, bb, r in cov if (i, r) in foreign): risky += 1; left = [i for i, bb, r in cov if (i, r) not in foreign and not (y + DY < bb[1] or y - DY > bb[4])]; fixed += 1 if True else 0
            rows.append(f'L{L} {mid} {ph} этап {si} {nm}: точка {x:.1f},{y:.1f},{z:.1f} набор {S:04x} накрывают {[(i, r, round(bb[1], 1), round(bb[4], 1)) for i, bb, r in cov]} ЧУЖИЕ {foreign} по высоте {high}')
        elif high:
            rows.append(f'L{L} {mid} {ph} этап {si} {nm}: точка {x:.1f},{y:.1f},{z:.1f} набор {S:04x} накрывают {len(cov)} клеток, по высоте чужих {high} (район в наборе — клетка загрузится, но чужой пол)')
print('\n'.join(rows)); print(f'ИТОГ: точек постановки персонажей {tot}; у {bad_old} старое правило ждёт клетку района вне набора этапа (новое её не ждёт); из них у {risky} чужая клетка ПО ВЫСОТЕ накрывает ноги (возможно, это единственный пол: новое правило покажет персонажа без загруженного пола — смотреть глазом)')
