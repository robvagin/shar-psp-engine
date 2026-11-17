#!/bin/bash
# Гейт окна Гомер 107b, пункт 1284 «посадка издалека всегда пешком к двери» (homer.c HS_TO_DOOR). Зелёный: сел сам, прошёл ≥ 3 м, ирисовых посадок 0.
# Красный (-DHOMERWALK1284_OLD=1): ирис мгновенно (1 посадка), прошёл < 1 м.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/h107b; mkdir -p "$OUT"; V=viewer
D=${SHR_DATA:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15}
SRC=(tests/homer107b/walk1284.c $V/homer.c $V/skin.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c $V/watchdog.c)
b() { cc -std=c11 -O2 -w -DSKIN_HOST "$@" -I $V "${SRC[@]}" -lm -o "$OUT/walk_$NAME" && "$OUT/walk_$NAME" "$D/level.shr" "$D/homer.shr" > "$OUT/walk_$NAME.txt"; }
NAME=new; b || { echo "FAIL: сборка"; exit 1; }; NAME=old; b -DHOMERWALK1284_OLD=1 || exit 1
g() { grep "^$2 " "$OUT/walk_$1.txt" | cut -d' ' -f2; }
echo "новое: сел $(g new in_car) за $(g new seconds) с, прошёл $(g new walked_m) м, ирисовых посадок $(g new iris_seats); старое: сел $(g old in_car) за $(g old seconds) с, прошёл $(g old walked_m) м, ирисовых посадок $(g old iris_seats)"
ge() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }; lt() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<b)}'; }
fail=0
{ [ "$(g new enter_ok)" = 1 ] && [ "$(g new in_car)" = 1 ] && [ "$(g new iris_seats)" = 0 ] && ge "$(g new walked_m)" 3.0; } || { echo "FAIL зелёный"; fail=1; }
{ [ "$(g old iris_seats)" = 1 ] && lt "$(g old walked_m)" 1.0; } && echo "красный HOMERWALK1284_OLD=1: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
