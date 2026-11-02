#!/bin/bash
# Круг 7 (g11, P11-14): порог частицы листвы по импульсу. Зелёный: 1500 кг при 0,5 м/с — частица есть, при 0,02 м/с (меньше 50 Н·с) нет, при 5 м/с есть. Красный: -DFOLIAGEIMP_SV_OLD=1 (порог скорости 3 м/с): на 0,5 м/с частицы нет.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-build/level.shr}"; OUT=build/foliageimp; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"; SRC="tests/foliageimp/host.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head; exit 1; }; "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DFOLIAGEIMP_SV_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: 0,5 м/с $(g green fx_1500_0p5), 0,02 м/с $(g green fx_1500_0p02), 5 м/с $(g green fx_1500_5); езд на 1,5 м/с $(g green slow_runs), с листвой $(g green slow_foliage)"
fail=0
[ "$(g green fx_1500_0p5)" = 1 ] && [ "$(g green fx_1500_0p02)" = 0 ] && [ "$(g green fx_1500_5)" = 1 ] || { echo "FAIL зелёный"; fail=1; }
[ "$(g old fx_1500_0p5)" = 0 ] && echo "красный FOLIAGEIMP_SV_OLD=1: упал, как должен — на 0,5 м/с частицы нет" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
