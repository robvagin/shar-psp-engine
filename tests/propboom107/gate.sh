#!/bin/bash
# 107б П11: взрыв задевает не больше 20 ближайших предметов. Хост на L1 (плотное место: предметов в 20 м больше 20). Зелёный: moved <= 20 при in_radius > 20. Красный (-DPROPBOOM107_OLD=1): moved == in_radius (все).   tests/propboom107/gate.sh
set -u; cd "$(dirname "$0")/../.."; SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/propboom107; mkdir -p $OUT
SRC=(tests/propboom107/host.c viewer/col.c viewer/props.c viewer/car.c viewer/stateprops.c viewer/coins.c)
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "${SRC[@]}" -lm -o $OUT/g || { echo "FAIL: сборка"; exit 1; }; cc -std=c11 -O2 -w -DSKIN_HOST -DPROPBOOM107_OLD=1 -I viewer "${SRC[@]}" -lm -o $OUT/r || exit 1
g() { grep "^$2 " $OUT/$1.txt | cut -d' ' -f2; }; $OUT/g $SET/level.shr > $OUT/g.txt; $OUT/r $SET/level.shr > $OUT/r.txt
echo "в радиусе $(g g in_radius): зелёный задето $(g g moved), красный задето $(g r moved)"; fail=0
[ "$(g g in_radius)" -gt 20 ] && [ "$(g g moved)" -le 20 ] && [ "$(g g moved)" -ge 1 ] || { echo "FAIL зелёный"; fail=1; }
[ "$(g r moved)" -gt 20 ] && echo "красный PROPBOOM107_OLD=1: задеты все, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
