#!/bin/bash
# Гейт окна Гомер 107b, пункт «наклон 30° на рельефе» (сверка Fable-2 В1; homer.c slopeIsTerrain): на реальных травяных склонах 33–50° (L1, L2, L3, L7 из выпуска 106, участки найдены сканом рельефа
# COL_TRI) Гомер идёт вверх стиком 2,5 с. Зелёный: набор высоты ≥ 3 м не меньше чем на 80% склонов. Красный: -DHOMERSLOPE107B_OLD=1 — предел 30° на всём, набор ровно 0,5 м (HOMER_SLOPE_RISE) везде.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/h107b; mkdir -p "$OUT"; V=viewer
D=${SHR_DATA:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15}
ARGS=("$D/homer.shr" "$D/level.shr" "$D/l2/level.shr" "$D/l3/level.shr" "$D/l7/level.shr")
for f in "${ARGS[@]}"; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
SRC=(tests/homer107b/slope.c $V/homer.c $V/skin.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c $V/watchdog.c)
b() { cc -std=c11 -O2 -w -DSKIN_HOST "$@" -I $V "${SRC[@]}" -lm -o "$OUT/slope_$NAME" && "$OUT/slope_$NAME" "${ARGS[@]}" > "$OUT/slope_$NAME.txt"; }
NAME=new; b || { echo "FAIL: сборка"; exit 1; }; NAME=old; b -DHOMERSLOPE107B_OLD=1 || exit 1
g() { grep "^$2 " "$OUT/slope_$1.txt" | cut -d' ' -f2; }
echo "новое: склонов $(g new slope_tested), поднялся $(g new slope_climbed), средний набор $(g new slope_mean_gain) м; старое: поднялся $(g old slope_climbed), средний набор $(g old slope_mean_gain) м"
fail=0
{ [ "$(g new slope_tested)" -ge 10 ] && [ $(( $(g new slope_climbed) * 10 )) -ge $(( $(g new slope_tested) * 8 )) ]; } || { echo "FAIL зелёный"; fail=1; }
{ [ "$(g old slope_climbed)" = 0 ] && [ "$(g old slope_tested)" = "$(g new slope_tested)" ]; } && echo "красный HOMERSLOPE107B_OLD=1: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
