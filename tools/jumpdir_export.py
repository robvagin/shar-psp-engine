#!/usr/bin/env python3
"""Курс локаторов зон прыжка (сверка g01-cars, круг 5, P01-07): в блоке JMP1 уровня у зоны только оси её объёма, а они у большинства зон
единичные и курс въезда не несут. Движку нужен курс самого локатора (строка его матрицы), чтобы разгон зоны включался только у машины,
въехавшей по ходу зоны. Отдельный маленький файл на уровень, `jumpdir-l<N>.shr`, формат JZD1 (little endian):

  заголовок 16 Б: 'JZD1', u32 версия (1), u32 n, u32 0
  запись 32 Б: char name[16] имя локатора зоны; f32 dx, dz курс зоны в плане (оси движка, z отзеркален, длина 1); f32 0, 0

Зоны без матрицы локатора в файл не попадают (движок для них проверку курса не делает). level.shr не меняется.
Выключатель JUMPDIR_OLD=1 в окружении — файл не пишется (прежнее поведение).
    python tools/jumpdir_export.py --level 1 --out build/jumpdir-l1.shr"""
import argparse, math, os, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/missions')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import jumps_export as J                      # noqa: E402
from p3dlocs import read_locators             # noqa: E402


def zones(level):
    out, seen = [], set()
    for f in J.script_files(level) + [p for p in J.scene_files(level) if p.exists()]:
        locs, err = read_locators(f, str(f.relative_to(J.GAME)))
        if err: continue
        for L in locs:
            if L.type != 0 or L.data != J.EV_JUMPZONE or not L.triggers or not getattr(L, 'mat', None): continue
            key = (L.name, round(L.pos[0], 2), round(L.pos[2], 2))
            if key in seen: continue
            seen.add(key)
            m = L.mat; dx, dz = m[8], -m[10]            # строка 2 матрицы локатора, z отзеркален
            n = math.hypot(dx, dz)
            if n < 1e-4: continue
            out.append((L.name, dx / n, dz / n))
    return out


def build(level):
    zs = zones(level)
    b = b'JZD1' + struct.pack('<3I', 1, len(zs), 0)
    for name, dx, dz in zs: b += struct.pack('<16s4f', name.encode('latin1')[:15], dx, dz, 0.0, 0.0)
    return b, zs


if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('--level', type=int, default=1); ap.add_argument('--out')
    a = ap.parse_args()
    if os.environ.get('JUMPDIR_OLD') == '1': print('jumpdir: JUMPDIR_OLD=1, файл не пишется'); sys.exit(0)
    b, zs = build(a.level)
    if a.out:
        Path(a.out).parent.mkdir(parents=True, exist_ok=True); Path(a.out).write_bytes(b)
    print(f'jumpdir: уровень {a.level}, зон с курсом {len(zs)}, {len(b)} Б' + (f' -> {a.out}' if a.out else ''))
    for n, dx, dz in zs: print(f'  {n:16s} ({dx:+.2f}, {dz:+.2f})')
