#!/bin/bash
# Сверка g10-flow 02.11: красный теперь -DGAME_EXIT_PS2_OLD=1 (прежнее: ножной тормоз, выход при 5 м/с, △ ещё раз — отмена).
# Правка 168: △ на ходу — машина тормозит и Гомер выходит (tests/controls/exit_host.c). Сначала красный: -DGAME_EXIT_ASSIST=0
# (прежнее: на ходу △ ничего не делает) обязан упасть; потом зелёный.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/controls; mkdir -p "$OUT"
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots"
ARGS="build/level.shr build/homer.shr build/missions/level01.msn"
$CC -DGAME_EXIT_PS2_OLD=1 tests/controls/exit_host.c $BOTS_LIB -lm -o "$OUT/exit_red" 2> "$OUT/exit_build.txt" || { echo "FAIL: сборка (красный)"; head -5 "$OUT/exit_build.txt"; exit 1; }
if "$OUT/exit_red" $ARGS > "$OUT/exit_red.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН"; red=0; else echo "красный: $(grep -c 'CHECK.*FAIL' "$OUT/exit_red.txt") провал(а): $(grep -m1 'CHECK.*FAIL' "$OUT/exit_red.txt" | cut -c1-100)"; red=1; fi
$CC tests/controls/exit_host.c $BOTS_LIB -lm -o "$OUT/exit" 2> "$OUT/exit_build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/exit_build.txt"; exit 1; }
echo "зелёный:"; "$OUT/exit" $ARGS | grep -E "CHECK|RESULT"; green=${PIPESTATUS[0]}
[ $red = 1 ] && [ $green = 0 ] && echo "PASS (красный пойман, зелёный сошёлся)" || { echo "FAIL"; exit 1; }
