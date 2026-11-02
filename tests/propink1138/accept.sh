#!/bin/bash
# №1138 (g11): в мультике обводка у предметов убрана, у будки осталась. Хост на данных L1–L7: обводятся только типы-будки (число равно числу типов-будок, и оно больше нуля), стейтпропов-предметов без обводки большинство.
# Красный: -DPROPINK1138_OLD=1 — обводятся все типы.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/propink1138; mkdir -p "$OUT"; V=viewer
T=$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15
LV=(build/level.shr $T/l2/level.shr $T/l3/level.shr $T/l4/level.shr $T/l5/level.shr $T/l6/level.shr $T/l7/level.shr)
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V "$@" tests/propink1138/host.c $V/props.c $V/stateprops.c $V/col.c $V/car.c $V/coins.c -lm -o "$OUT/$NAME" && "$OUT/$NAME" "${LV[@]}" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DPROPINK1138_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: типов $(g green types), будок $(g green booth_types), с обводкой $(g green wanted_types); экземпляров $(g green instances), с обводкой $(g green wanted_instances); старое: с обводкой типов $(g old wanted_types)"
ok() { [ "$(g $1 booth_types)" -gt 0 ] && [ "$(g $1 wanted_types)" = "$(g $1 booth_types)" ] && [ "$(g $1 wanted_instances)" -lt "$(( $(g $1 instances) / 4 ))" ]; }
fail=0; ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ PROPINK1138_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный PROPINK1138_OLD=1: упал, как должен — обводятся все $(g old wanted_types) типов"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
