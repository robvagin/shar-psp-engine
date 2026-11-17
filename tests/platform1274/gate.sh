#!/bin/bash
# №1274: хост-гейт переноса Гомера подвижной платформой (animobj.c) на уровнях L1-L7, настоящие данные. Зелёный: у каждой подвижной платформы (ход ≥ 1,5 м или поворот, не створка) ступни за весь клип остаются на ящике;
# платформы АЭС L1 и L4 (4+4, ходы 4, 4, 3,1, 14 м) найдены. Красный (обязан упасть): aoRide1274Old = 1 — ни одна не держит.  tests/platform1274/gate.sh
set -u; cd "$(dirname "$0")/../.."
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/plat1274; mkdir -p $OUT
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer tests/platform1274/gate.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/animobj.c viewer/car.c viewer/coins.c -lm -o $OUT/gate || { echo "FAIL: сборка"; exit 1; }
fail=0; tot=0; hold=0; holdOld=0
for L in 1 2 3 4 5 6 7; do F=$SET/l$L/level.shr; [ $L = 1 ] && F=$SET/level.shr
  n=$($OUT/gate $F | tail -1); o=$($OUT/gate $F 1 | tail -1); echo "L$L: $n | прежнее: $o"
  a=$(echo "$n" | sed -E 's/.*\) ([0-9]+), удержало ([0-9]+),.*/\1 \2/'); set -- $a; tot=$((tot+$1)); hold=$((hold+$2))
  b=$(echo "$o" | sed -E 's/.*\) ([0-9]+), удержало ([0-9]+),.*/\1 \2/'); set -- $b; holdOld=$((holdOld+$2))
  [ "$L" = 1 -o "$L" = 4 ] && { [ "$(echo "$n" | sed -E 's/.*\) ([0-9]+),.*/\1/')" -ge 4 ] || { echo "FAIL: платформы АЭС L$L не найдены"; fail=1; }; }
done
echo "платформ всего $tot, удержало $hold; прежнее удержало $holdOld"
[ "$hold" = "$tot" ] && [ "$tot" -ge 9 ] || { echo "FAIL: удержали не все"; fail=1; }
[ "$holdOld" -ge "$tot" ] && { echo "КРАСНЫЙ НЕ ПОЙМАН: прежнее тоже держит"; fail=1; } || echo "красный aoRide1274Old=1: упал, как должен (держит $holdOld из $tot)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
