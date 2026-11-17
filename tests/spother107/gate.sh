#!/bin/bash
# 107б П3: гейт «чужая машина бьёт стейтпропы» (хост, настоящие данные L1-L7). Зелёный: из экземпляров, которые сменили состояние от машины игрока (ref), чужая машина меняет такие же (other == ref), а монеты/шкала Hit & Run/последнее касание игрока у чужой не двигаются (stray 0).
# Красный (обязан упасть): -DSPOTHER107_OLD=1 — чужая машина сквозь стейтпропы (other 0).  tests/spother107/gate.sh
set -u; cd "$(dirname "$0")/../.."
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/spother107; mkdir -p $OUT
SRC=(tests/spother107/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c)
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "${SRC[@]}" -lm -o $OUT/green || { echo "FAIL: сборка"; exit 1; }
cc -std=c11 -O2 -w -DSKIN_HOST -DSPOTHER107_OLD=1 -I viewer "${SRC[@]}" -lm -o $OUT/red || { echo "FAIL: сборка красного"; exit 1; }
fail=0; totRef=0; totOther=0; totRedOther=0
for L in 1 2 3 4 5 6 7; do F=$SET/l$L/level.shr; [ $L = 1 ] && F=$SET/level.shr
  g() { grep "^$2 " $OUT/$1.txt | cut -d' ' -f2; }
  $OUT/green $F 80 > $OUT/green.txt; $OUT/red $F 80 > $OUT/red.txt
  echo "L$L: проверено $(g green tried), у игрока сменили состояние $(g green ref), у чужой $(g green other), утечек счётчиков игрока $(g green stray); красный: у чужой $(g red other)"
  [ "$(g green stray)" = 0 ] || { echo "  FAIL: счётчики игрока тронуты чужой машиной"; fail=1; }
  [ "$(g green other)" = "$(g green ref)" ] || { echo "  FAIL: чужая меняет не все"; fail=1; }
  totRef=$((totRef+$(g green ref))); totOther=$((totOther+$(g green other))); totRedOther=$((totRedOther+$(g red other)))
done
echo "всего ref $totRef, чужая $totOther, красный $totRedOther"
[ "$totRef" -ge 20 ] || { echo "FAIL: слишком мало ударов у эталона"; fail=1; }
[ "$totRedOther" -eq 0 ] && echo "красный SPOTHER107_OLD=1: упал, как должен (чужая сквозь стейтпропы)" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
