#!/bin/bash
# Шаг 8 (Z2): tests/z2/run.sh НАБОР — модель области города на езде по L4 и L7 (tools/z2_sim.py). Зелёное: блок по размеру нужна область ≤ 75 % от плит 16/32/64/128/192.
# Красный контроль: плиты 16/32/64/128/256 против нынешних плит не дают таких 25 % — тот же порог обязан не выполниться.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор}; cd $W; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
python3 - "$SET" <<'PY'
import sys; sys.path.insert(0, 'tools'); import z2_sim as Z
from pathlib import Path
S = Path(sys.argv[1]); rc = 0
def need(cells, mk, R, route):
    lo, hi = 1 << 20, 24 << 20
    while hi - lo > (128 << 10):
        mid = (lo + hi) // 2 // (64 << 10) * (64 << 10)
        if Z.run(cells, mk, mid, R, route) == 0: hi = mid
        else: lo = mid
    return hi
for lv in (4, 7):
    cells = Z.level_cells(S / f'l{lv}/level.shr'); route = Z.make_route(cells, 600, lv)[::4]
    s192 = need(cells, lambda c: Z.SlabPool(c, [16, 32, 64, 128, 192]), 220., route); s256 = need(cells, lambda c: Z.SlabPool(c, [16, 32, 64, 128, 256]), 220., route); d = need(cells, lambda c: Z.Direct(c), 220., route)
    ok = d <= .75 * s192; red = s256 <= .75 * s192   # красный: обязан быть False
    print(f'L{lv}: плиты 192 {s192 / 1048576:.2f} МБ, плиты 256 {s256 / 1048576:.2f}, блок по размеру {d / 1048576:.2f} (≤ 75 %: {"да" if ok else "НЕТ"}); красный контроль (плиты 256 тоже ≤ 75 %): {"не покраснел" if red else "ок, не выполняется"}')
    if not ok or red: rc = 1
print('PASS' if rc == 0 else 'FAIL'); sys.exit(rc)
PY
