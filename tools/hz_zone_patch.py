#!/usr/bin/env python3
"""№1042б: правка таблиц зон дальности hzzone.bin (HZN1) по списку мест tools/hz_zone_places.json без нового прогона бота: клетки, чей центр попал в прямоугольник
места, получают зону места (0 — 130, 1 — 150, 2 — 250), в том числе клетки «нет данных» (255). Формат места: {"<уровень>": [[x0, z0, x1, z1, зона], ...]}.
  hz_zone_patch.py SRC_DIR OUT_DIR [--places hz_zone_places.json]    SRC_DIR/l<N>/hzzone.bin -> OUT_DIR/l<N>/hzzone.bin (только уровни со списком мест)
Данные игры в git не кладём: результат — в build/<набор>/data, не в репозиторий."""
import sys, json, struct, argparse
from pathlib import Path
def main():
    ap = argparse.ArgumentParser(); ap.add_argument('src'); ap.add_argument('out')
    ap.add_argument('--places', default=str(Path(__file__).resolve().parent / 'hz_zone_places.json')); a = ap.parse_args()
    places = json.loads(Path(a.places).read_text()); rc = 0
    for lv, rects in places.items():
        sp = Path(a.src) / f'l{lv}' / 'hzzone.bin'
        if not sp.exists(): print(f'L{lv}: нет {sp}'); rc = 1; continue
        b = bytearray(sp.read_bytes())
        if b[:4] != b'HZN1': print(f'L{lv}: не HZN1'); rc = 1; continue
        w, h = struct.unpack_from('<II', b, 4); x0, z0, st = struct.unpack_from('<fff', b, 12); n = 0
        for gz in range(h):
            for gx in range(w):
                cx, cz = x0 + (gx + .5) * st, z0 + (gz + .5) * st
                for (rx0, rz0, rx1, rz1, md) in rects:
                    if rx0 <= cx <= rx1 and rz0 <= cz <= rz1:
                        b[24 + gz * w + gx] = md; n += 1; break
        op = Path(a.out) / f'l{lv}' / 'hzzone.bin'; op.parent.mkdir(parents=True, exist_ok=True); op.write_bytes(bytes(b)); print(f'L{lv}: клеток зоны выставлено {n} -> {op}')
    return rc
sys.exit(main())
