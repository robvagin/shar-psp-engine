#!/bin/bash
# Гейт №1082 (дым колёс при резком газе, g11): уровень пробуксовки на хосте. Зелёный: с места уровень выше 0,3, через 1 с он ниже, чем на 0,3 с (дым стихает по мере
# разгона), тормоз задним ходом даёт уровень выше 0. Красный (обязан упасть): -DWHEELSMOKE1082_OLD=1 — уровня нет совсем (как в TEST82, дым шёл на полную всегда).
#   tests/wheelsmoke1082/accept.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-build/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
OUT=build/wheelsmoke1082; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"
SRC="tests/wheelsmoke1082/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; grep -E "error|Undefined|\"_" "$OUT/$NAME.build" | head -5; exit 1; }; "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DWHEELSMOKE1082_OLD=1; NAME=latch; b -DBURNLATCH_SV_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
ok() { awk -v a="$(g $1 fwd_03)" -v z="$(g $1 fwd_1)" -v r="$(g $1 rev_max)" 'BEGIN{exit !(a > .3 && z < a && r > 0)}'; }
echo "зелёный: старт $(g green fwd_03), 1 с $(g green fwd_1), 3 с $(g green fwd_3), 6 с $(g green fwd_6), назад $(g green rev_max), большой клуб $(g green big)"
fail=0
ok green || { echo "FAIL зелёный: уровень пробуксовки не по закону"; fail=1; }
ok old && { echo "КРАСНЫЙ WHEELSMOKE1082_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный WHEELSMOKE1082_OLD=1: упал, как должен — старт $(g old fwd_03), назад $(g old rev_max)"
# круг 7: огонь из-под колёс гаснет, когда таймер рывка убыл ниже 2,5 с, хотя пробуксовка продолжается; защёлка №1082 держала его до конца пробуксовки
echo "рывок: огонь на ручнике $(g green big_hb), после отпускания $(g green big_after), кадров пробуксовки после $(g green burn_after_frames), огня при убывшем таймере $(g green big_stale)"
[ "$(g green big_hb)" = 1 ] && [ "$(g green big_stale)" = 0 ] && [ "$(g green burn_after_frames)" -gt 0 ] || { echo "FAIL зелёный: рывок"; fail=1; }
[ "$(g latch big_stale)" -gt 0 ] && echo "красный BURNLATCH_SV_OLD=1: упал, как должен — огня при убывшем таймере $(g latch big_stale) кадров" || { echo "КРАСНЫЙ BURNLATCH_SV_OLD НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
