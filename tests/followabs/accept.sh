#!/bin/bash
# Круг 7 (g11): объём за суставом — положение сустава целиком без поворота экземпляра. Данные: L1, L2, L7 из набора TEST83/84 (level.shr и objnames.shr рядом). Зелёный: у автомата и паука центр объёма
# отличается от запечённого на положение сустава в покое (больше 0,2 м), у всех автоматов вектор один и тот же (поворот экземпляра не влияет). Красный: -DFOLLOWABS1107_OLD=1 — разницы нет (сдвиг от кадра 0).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/followabs; mkdir -p "$OUT"; V=viewer
T=$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15; S=$HOME/Downloads/shar-psp-integ/build/t84stage
ARGS=(build/level.shr "$S/objnames.shr" "$T/l2/level.shr" "$S/l2/objnames.shr" "$T/l7/level.shr" "$S/l7/objnames.shr")
for f in "${ARGS[@]}"; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V"; SRC="tests/followabs/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" && "$OUT/$NAME" "${ARGS[@]}" > "$OUT/$NAME.txt"; }
NAME=green; b || { echo "FAIL: сборка"; exit 1; }; NAME=old; b -DFOLLOWABS1107_OLD=1 || exit 1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: типов со следованием $(g green types), экземпляров $(g green instances), со смещением больше 0,2 м $(g green nonzero), один вектор у автоматов $(g green same_vector_for_vending); старое: со смещением $(g old nonzero)"
fail=0
[ "$(g green nonzero)" -gt 0 ] && [ "$(g green same_vector_for_vending)" = 1 ] || { echo "FAIL зелёный"; fail=1; }
[ "$(g old nonzero)" = 0 ] && echo "красный FOLLOWABS1107_OLD=1: упал, как должен — смещений нет" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
