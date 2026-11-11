#!/bin/bash
# Сверка с исходником, окно g03-aicars (02.11.2026): tests/aicar/sverka_host.c. Сначала красный (прежнее поведение: -DMC_SVERKA_OLD=1
# -DPOLICE_STAND27_OLD=1) — каждая проверка обязана упасть; потом зелёный. Без эмулятора и без данных диска.
cd "$(dirname "$0")/../.."
OUT=build/aicar; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
SRC="tests/aicar/sverka_host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/watchdog.c"
$CC -DMC_SVERKA_OLD=1 -DPOLICE_STAND27_OLD=1 $SRC -lm -Wl,-U,_uiFlash -o "$OUT/sverka_red" 2> "$OUT/sverka_build.txt" || { echo "FAIL: сборка (красный)"; head -5 "$OUT/sverka_build.txt"; exit 1; }
"$OUT/sverka_red" > "$OUT/sverka_red.txt" 2>&1; nred=$(grep -c "^CHECK.*FAIL" "$OUT/sverka_red.txt"); ntot=$(grep -c "^CHECK" "$OUT/sverka_red.txt")
echo "красный (прежнее поведение): упало $nred из $ntot"; grep "^CHECK.*OK" "$OUT/sverka_red.txt" | cut -c1-150
$CC $SRC -lm -Wl,-U,_uiFlash -o "$OUT/sverka" 2> "$OUT/sverka_build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/sverka_build.txt"; exit 1; }
echo "зелёный:"; "$OUT/sverka" | tee "$OUT/sverka_green.txt" | grep -E "CHECK|RESULT"; green=${PIPESTATUS[0]}
[ "$nred" -ge "$ntot" ] && [ "$green" = 0 ] && echo "PASS (красный поймал все проверки, зелёный сошёлся)" || { echo "FAIL (красный упал $nred из $ntot, зелёный код $green)"; exit 1; }
