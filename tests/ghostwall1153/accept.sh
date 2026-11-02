#!/bin/bash
# №1140/1153 (g11): труба-культверт у телефонной будки L7 H4-9 («прозрачный столб»): после правки машина на 10–15 м/с проезжает место трубы без остановки (за трубой не меньше 3 м/с, жёсткого удара нет).
# Красный: -DGHOSTWALL1153_OLD=1 — короткая закопанная труба остаётся стеной (остановка, удар ~17 м/с). Данные: L7 из набора TEST83 (test71tour).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/ghostwall1153; mkdir -p "$OUT"; V=viewer
L="${1:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15/l7/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V "$@" tests/ghostwall1153/host.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c -lm -o "$OUT/$NAME" && "$OUT/$NAME" "$L" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DGHOSTWALL1153_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: заездов $(g green runs), встало $(g green stopped), жёсткий удар до $(g green hard_max), скорость за трубой от $(g green v_after_min); старое: встало $(g old stopped), удар до $(g old hard_max)"
ok() { [ "$(g $1 runs)" -gt 0 ] && [ "$(g $1 stopped)" = 0 ] && awk -v h="$(g $1 hard_max)" 'BEGIN{exit !(h < 5)}'; }
fail=0; ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ GHOSTWALL1153_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный GHOSTWALL1153_OLD=1: упал, как должен"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
