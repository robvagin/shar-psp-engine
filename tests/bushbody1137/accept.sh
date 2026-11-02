#!/bin/bash
# №1137/1154 (g11): кусты-стейтпропы без тела получили объём. Хост на данных L1–L7: кустов без объёма 0 (раньше ~20 на уровень); куст у L7 J9-5 (156,7; 159,9) на 3 м/с держит машину (не проехала насквозь), на 12 м/с продавливается.
# Красный: -DBUSHFALL1137_OLD=1 — тел нет (bare > 0, на 3 м/с машина проезжает куст насквозь).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/bushbody1137; mkdir -p "$OUT"; V=viewer
T=$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15
LV=(build/level.shr $T/l2/level.shr $T/l3/level.shr $T/l4/level.shr $T/l5/level.shr $T/l6/level.shr $T/l7/level.shr)
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V "$@" tests/bushbody1137/host.c $V/props.c $V/stateprops.c $V/col.c $V/car.c $V/coins.c -lm -o "$OUT/$NAME" && "$OUT/$NAME" "${LV[@]}" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DBUSHFALL1137_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: кустов $(g green bushes), без тела $(g green bare); куст J9-5 (индекс $(g green bush_found)): 3 м/с проехала $(g green slow_crossed), 12 м/с проехала $(g green fast_crossed); старое: без тела $(g old bare), 3 м/с проехала $(g old slow_crossed)"
ok() { [ "$(g $1 bush_found)" -ge 0 ] && [ "$(g $1 bare)" = 0 ] && [ "$(g $1 slow_crossed)" = 0 ] && [ "$(g $1 fast_crossed)" = 1 ]; }
fail=0; ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ BUSHFALL1137_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный BUSHFALL1137_OLD=1: упал, как должен"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
