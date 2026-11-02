#!/bin/bash
# Гейт №1096 (контур предмета мультика не мигает), хост на Маке. Зелёный: предмет у дороги тронула машина трафика — признак «контур закреплён» (inkTouch) 0; тронула машина игрока — 1 и остаётся 1 после
# засыпания предмета; регулятор кадра контур предметов не отключает (gov_off 1). Красный (обязан упасть): -DINK1096_OLD=1 — как в TEST83 (регулятор отключает, признака нет).
#   tests/ink1096/accept.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-build/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
OUT=build/ink1096; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"
SRC="tests/ink1096/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head -5; exit 1; }; "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DINK1096_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
ok() { [ "$(g $1 chosen)" -ge 0 ] && [ "$(g $1 traf_kicked)" = 1 ] && [ "$(g $1 traf_ink)" = 0 ] && [ "$(g $1 player_ink)" = 1 ] && [ "$(g $1 player_asleep)" = 1 ] && [ "$(g $1 player_ink_after_sleep)" = 1 ] && [ "$(g $1 gov_off)" = 1 ]; }
echo "зелёный: предмет $(g green chosen); трафик задел $(g green traf_kicked), контур $(g green traf_ink); игрок: задел $(g green player_kicked), контур $(g green player_ink), уснул $(g green player_asleep), контур после сна $(g green player_ink_after_sleep); регулятор не гасит $(g green gov_off)"
fail=0
ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ INK1096_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный INK1096_OLD=1: упал, как должен — контур игрока $(g old player_ink), регулятор не гасит $(g old gov_off)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
