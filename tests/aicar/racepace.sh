#!/bin/bash
# 09.11: ВЫВЕДЕН ИЗ ОБЯЗАТЕЛЬНЫХ (решение дирижёра): красный на правильном коде, причина в шапке racepace_host.c; проверки закрыты tests/aicar/sverka*.sh
# (race25, first2s, race-f). Измерение по настоящим дорогам — tests/aicar/racereal.sh (без порогов, пока нет темпа игрока PS2 по кадрам).
# Темп соперника гонки против записи PS2 (аудит 09.10 п. 10, правка 347; tests/aicar/racepace_host.c).
# Сначала красный: -DMC_CATCHUP_SPEED_OLD=1 (множитель догонялки к своей TopSpeed .con) обязан упасть; потом зелёный.
cd "$(dirname "$0")/../.."
OUT=build/aicar; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
SRC="tests/aicar/racepace_host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/watchdog.c"
$CC -DMC_CATCHUP_SPEED_OLD=1 $SRC -lm -o "$OUT/racepace_red" 2> "$OUT/racepace_build.txt" || { echo "FAIL: сборка (красный)"; head -5 "$OUT/racepace_build.txt"; exit 1; }
if "$OUT/racepace_red" "$OUT/racepace_red.tsv" > "$OUT/racepace_red.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН"; red=0; else echo "красный: $(grep -m1 'CHECK.*FAIL' "$OUT/racepace_red.txt" | cut -c1-200)"; red=1; fi
$CC $SRC -lm -o "$OUT/racepace" 2> "$OUT/racepace_build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/racepace_build.txt"; exit 1; }
echo "зелёный:"; "$OUT/racepace" "$OUT/racepace.tsv" | tee "$OUT/racepace_green.txt" | grep -E "путь|CHECK|RESULT"; green=${PIPESTATUS[0]}
[ $red = 1 ] && [ $green = 0 ] && echo "PASS (красный пойман, зелёный сошёлся)" || { echo "FAIL"; exit 1; }
