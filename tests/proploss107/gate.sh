#!/bin/bash
# 107б П5: потеря скорости о фонарь 1000 кг по записи PS2 (1,0 + 0,28·v), оба пути (простой предмет L1, стейтпроп L5), v = 5,3 / 10,3 / 20,2 м/с. Зелёный: обе потери в пределах 20 % записи при фактической скорости перед ударом (машина без газа успевает притормозить за последние 5 м). Красный (-DPROPLOSS107_OLD=1): хотя бы одна вне.
#   tests/proploss107/gate.sh
set -u; cd "$(dirname "$0")/../.."
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/proploss107; mkdir -p $OUT
SRC=(tests/proploss107/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c)
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "${SRC[@]}" -lm -o $OUT/green || { echo "FAIL: сборка"; exit 1; }
cc -std=c11 -O2 -w -DSKIN_HOST -DPROPLOSS107_OLD=1 -I viewer "${SRC[@]}" -lm -o $OUT/red || { echo "FAIL: сборка красного"; exit 1; }
chk() { python3 - "$1" <<'PY'
import sys, re
ok = True
for l in open(sys.argv[1]):
    m = re.match(r'v\d (\S+) propsv (\S+) props (\S+) spv (\S+) sp (\S+)', l)
    if m:
        v, pv, p, sv, s = map(float, m.groups()); rp = 1 + .28 * pv; rs = 1 + .28 * sv
        print('  v=%.1f: простой при %.2f м/с потеря %.2f, запись %.2f (%+.0f %%); стейтпроп при %.2f м/с потеря %.2f, запись %.2f (%+.0f %%)' % (v, pv, p, rp, 100 * (p / rp - 1), sv, s, rs, 100 * (s / rs - 1)))
        if abs(p / rp - 1) > .2 or abs(s / rs - 1) > .2: ok = False
sys.exit(0 if ok else 1)
PY
}
$OUT/green $SET/level.shr $SET/l5/level.shr > $OUT/green.txt; $OUT/red $SET/level.shr $SET/l5/level.shr > $OUT/red.txt
echo "зелёный:"; chk $OUT/green.txt; g=$?; echo "красный (PROPLOSS107_OLD=1):"; chk $OUT/red.txt; r=$?
[ $g = 0 ] && [ $r != 0 ] && { echo "красный упал, как должен"; echo PASS; exit 0; }; echo "FAIL (зелёный $g, красный $r)"; exit 1
