#!/bin/bash
# Гейт №1103 (сбитые предметы лежат до выгрузки зоны, как в оригинале), хост на Маке, L1. Зелёный: сбитый предмет лежит там же и на 100 м, и на 300 м от игрока 120 с, не спрятан и не вернулся домой;
# за PROP_ZONE_R (450 м) возвращается домой; машина проезжает по лежащему 3 раза без потери скорости (не меньше 70 %) и его не двигает. Красные (обязаны упасть): -DPROPLIE_SV_OLD=1 (домой через 5 с дальше 60 м,
# как TEST81) и -DPROPONCE_SV_OLD=1 (лежащий снова бьётся машиной).
#   tests/proplie1103/accept.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-build/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
OUT=build/proplie1103; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"
SRC="tests/proplie1103/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head -5; exit 1; }; "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=lie; b -DPROPLIE_SV_OLD=1; NAME=once; b -DPROPONCE_SV_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
ok() { [ "$(g $1 chosen)" -ge 0 ] && [ "$(g $1 kicked)" = 1 ] && [ "$(g $1 lies_away)" = 1 ] && [ "$(g $1 still_100m)" = 1 ] && [ "$(g $1 hidden_100m)" = 0 ] && [ "$(g $1 home_100m)" = 0 ] && [ "$(g $1 home_300m)" = 0 ] && [ "$(g $1 hidden_300m)" = 0 ] && [ "$(g $1 home_beyond_zone)" = 1 ] \
  && awk -v r="$(g $1 speed_ratio)" -v m="$(g $1 moved_by_cars)" 'BEGIN{exit !(r >= .7 && m < .05)}'; }
echo "зелёный: предмет $(g green chosen); лежит на месте на 100 м $(g green still_100m), дома на 100/300 м $(g green home_100m)/$(g green home_300m), за зоной дома $(g green home_beyond_zone); скорость при проезде $(g green speed_ratio), сдвиг машинами $(g green moved_by_cars) м"
fail=0
ok green || { echo "FAIL зелёный"; fail=1; }
ok lie && { echo "КРАСНЫЙ PROPLIE_SV_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный PROPLIE_SV_OLD=1: упал, как должен — дома на 100 м $(g lie home_100m), спрятан $(g lie hidden_100m)"
ok once && { echo "КРАСНЫЙ PROPONCE_SV_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный PROPONCE_SV_OLD=1: упал, как должен — сдвиг машинами $(g once moved_by_cars) м, скорость $(g once speed_ratio)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
