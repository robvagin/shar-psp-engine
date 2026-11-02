#!/bin/bash
# Гейт №1102 (лифты и платформы только по △), хост на Маке. Зелёный: ни один объект PlayAnim/Toggle не стартует сам, когда Гомер стоит на нём (auto_started 0);
# у каждого есть подсказка △ в его объёме (no_hint 0) и △ запускает клип. Красный (обязан упасть): -DLIFTTRI1102_OLD=1 — автозапуск при наступании, как правка 519 (auto_started > 0).
#   tests/lifttri1102/accept.sh [LEVEL.shr ...]   (по умолчанию build/level.shr и l2..l7 из набора, если есть)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
LV=("$@"); [ ${#LV[@]} -eq 0 ] && { LV=(build/level.shr); for n in 2 3 4 5 6 7; do for d in build/levels/l$n $HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15/l$n; do [ -f $d/level.shr ] && { LV+=($d/level.shr); break; }; done; done; }
for f in "${LV[@]}"; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
OUT=build/lifttri1102; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"
SRC="tests/lifttri1102/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c $V/animobj.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head -5; exit 1; }; "$OUT/$NAME" "${LV[@]}" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DLIFTTRI1102_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: объектов $(g green objects), стартуют сами $(g green auto_started), без подсказки △ $(g green no_hint), △ запускает $(g green press_started); старый режим: сами $(g old auto_started)"
fail=0
[ "$(g green objects)" -gt 0 ] && [ "$(g green auto_started)" = 0 ] && [ "$(g green no_hint)" = 0 ] && [ "$(g green press_started)" = "$(g green objects)" ] || { echo "FAIL зелёный"; fail=1; }
[ "$(g old auto_started)" -gt 0 ] && echo "красный LIFTTRI1102_OLD=1: упал, как должен — сами стартуют $(g old auto_started) из $(g old objects)" || { echo "КРАСНЫЙ LIFTTRI1102_OLD НЕ ПОЙМАН: автозапуска нет"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
