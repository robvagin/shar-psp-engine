#!/bin/bash
# Догонялка соперника гонки по доле пройденной гонки (разбор ночи 02.10 для приёмки L1, 03.10; tests/aicar/catchup_host.c).
# Сначала красный: -DMC_CATCHUP_GEO_OLD=1 (прежняя мера: по прямой до любой точки пути соперника) обязан упасть; потом зелёный.
cd "$(dirname "$0")/../.."
OUT=build/aicar; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
SRC="tests/aicar/catchup_host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/watchdog.c"
$CC -DMC_CATCHUP_GEO_OLD=1 $SRC -lm -o "$OUT/catchup_red" 2> "$OUT/catchup_build.txt" || { echo "FAIL: сборка (красный)"; head -5 "$OUT/catchup_build.txt"; exit 1; }
if "$OUT/catchup_red" > "$OUT/catchup_red.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН"; red=0; else echo "красный: $(grep -m1 'CHECK.*FAIL' "$OUT/catchup_red.txt" | cut -c1-160)"; red=1; fi
$CC $SRC -lm -o "$OUT/catchup" 2> "$OUT/catchup_build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/catchup_build.txt"; exit 1; }
echo "зелёный:"; "$OUT/catchup" | tee "$OUT/catchup_green.txt" | grep -E "CHECK|RESULT"; green=${PIPESTATUS[0]}
[ $red = 1 ] && [ $green = 0 ] && echo "PASS (красный пойман, зелёный сошёлся)" || { echo "FAIL"; exit 1; }
