#!/bin/bash
# Гейт цены «машина задела предмет» (№1086 и круг 6 g11), хост на Маке, без эмулятора.
# Зелёный: проходов по записям предметов на кадр не больше числа предметов (шаг физики и список «рядом» — одним проходом),
# обходов проверки касания на кадр при 14 чужих машинах не больше 15 % от «все машины × все предметы»; поведение то же (отпечаток положений
# предметов и число тронутых) что у отдельного прохода списка (-DPROPPASS1106_OLD=1). Красные (обязаны упасть): PROPPASS1106_OLD=1 — второй проход
# (проходов вдвое больше), PROPCOST1086_OLD=1 — как TEST82 (обходов в десятки раз больше).  Запуск: tests/props1086/accept.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-build/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
OUT=build/props1086; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -I $V"
SRC="tests/props1086/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; head -5 "$OUT/$NAME.build"; exit 1; }; "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b
NAME=pass_old; b -DPROPPASS1106_OLD=1
NAME=cost_old; b -DPROPCOST1086_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
n=$(grep -m1 "^предметов" "$OUT/green.txt" | cut -d' ' -f2)
okp() { awk -v a="$1" -v n="$n" 'BEGIN{exit !(a <= n + .5)}'; }
okh() { awk -v a="$1" -v n="$n" 'BEGIN{exit !(a <= .15 * 15 * n)}'; }
fail=0
echo "предметов $n; зелёный: проходов/кадр $(g green pass_per_frame), обходов касания/кадр $(g green hit_per_frame), тронуто $(g green kicked)"
okp "$(g green pass_per_frame)" || { echo "FAIL зелёный: проходов больше числа предметов"; fail=1; }
okh "$(g green hit_per_frame)" || { echo "FAIL зелёный: обходов касания слишком много"; fail=1; }
[ "$(g green hash)" = "$(g pass_old hash)" ] && [ "$(g green kicked)" = "$(g pass_old kicked)" ] || { echo "FAIL зелёный: поведение не то же, что у отдельного прохода (отпечаток $(g green hash) / $(g pass_old hash))"; fail=1; }
okp "$(g pass_old pass_per_frame)" && { echo "КРАСНЫЙ PROPPASS1106_OLD НЕ ПОЙМАН: проходов $(g pass_old pass_per_frame)"; fail=1; } || echo "красный PROPPASS1106_OLD=1: упал, как должен — проходов/кадр $(g pass_old pass_per_frame) при $n предметах"
okh "$(g cost_old hit_per_frame)" && { echo "КРАСНЫЙ PROPCOST1086_OLD НЕ ПОЙМАН: обходов $(g cost_old hit_per_frame)"; fail=1; } || echo "красный PROPCOST1086_OLD=1: упал, как должен — обходов касания/кадр $(g cost_old hit_per_frame)"
[ $fail -eq 0 ] && { echo "PASS"; exit 0; }; echo "FAIL"; exit 1
