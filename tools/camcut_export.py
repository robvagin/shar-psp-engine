#!/usr/bin/env python3
"""№1119, код Unlock All Cameras (вид wreckless): локаторы-события «смена камеры» (тип «событие», данные 1) уровня — место камеры и объёмы входа (art/l<N>*.p3d, tools/missions/p3dlocs.py).
На диске они есть у уровней 1–3 (54, 50, 44), у 4–7 нет. Файл camcut-l<N>.shr (маленький, читается при загрузке уровня): 'CUT1', u32 версия 1, u32 уровень, u32 число; запись 208 Б:
f32 pos[3] (координаты движка, z как у level.shr), u32 число объёмов (≤ 3), три слота по 64 Б: u32 форма (0 шар, 1 коробка), f32 центр[3], f32 половина[3] (у шара [0] — радиус), f32 оси[9].
Запуск: python tools/camcut_export.py --level 1 --out build/camcut-l1.shr"""
import argparse, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/missions'))
from p3dlocs import read_locators   # noqa: E402
GAME = ROOT / 'hit-and-run-web' / 'source' / 'game'
REC = 208

def cuts(level):
    out = []
    for f in sorted((GAME / 'art').glob('l%d*.p3d' % level)):
        try: locs = read_locators(f, f.name)[0]
        except Exception: continue
        for l in locs:
            if l.type == 0 and l.data == 1 and l.triggers: out.append(l)
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1); ap.add_argument('--out', default=None)
    a = ap.parse_args()
    out = Path(a.out or ROOT / 'build/camcut-l%d.shr' % a.level)
    ls = cuts(a.level); body = bytearray()
    for l in ls:
        r = struct.pack('<3fI', *l.pos, min(3, len(l.triggers)))
        for shape, c, h, ax in l.triggers[:3]:
            r += struct.pack('<I3f3f9f', shape, *c, *h, *[v for row in ax for v in row])
        r += bytes(REC - len(r)); body += r
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b'CUT1' + struct.pack('<3I', 1, a.level, len(ls)) + bytes(body))
    print('camcut: уровень %d, смен камеры %d -> %s (%d Б)' % (a.level, len(ls), out, 16 + len(body)))

if __name__ == '__main__': main()
