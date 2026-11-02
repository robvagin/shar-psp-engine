#!/bin/bash
# Круг 6, окно g03-aicars: tests/aicar/sverka6_host.c. Красный — прежнее поведение (-DMC_SVERKA_OLD=1 -DPOLICE_STAND27_OLD=1): все проверки обязаны упасть; зелёный проходит.
cd "$(dirname "$0")/../.."
OUT=build/aicar; mkdir -p "$OUT"; V=viewer
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
SRC="tests/aicar/sverka6_host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c"
LD="-lm -Wl,-U,_uiFlash"
$CC -DMC_SVERKA_OLD=1 -DPOLICE_STAND27_OLD=1 $SRC $LD -o "$OUT/sverka6_red" 2> "$OUT/sverka6_build.txt" || { echo "FAIL: сборка (красный)"; grep -E "error" -A3 "$OUT/sverka6_build.txt" | head; exit 1; }
"$OUT/sverka6_red" > "$OUT/sverka6_red.txt" 2>&1; nred=$(grep -c "^CHECK.*FAIL" "$OUT/sverka6_red.txt"); ntot=$(grep -c "^CHECK" "$OUT/sverka6_red.txt")
echo "красный (прежнее поведение): упало $nred из $ntot"; grep "^CHECK.*OK" "$OUT/sverka6_red.txt" | cut -c1-170
$CC $SRC $LD -o "$OUT/sverka6" 2> "$OUT/sverka6_build.txt" || { echo "FAIL: сборка"; grep -E "error" -A3 "$OUT/sverka6_build.txt" | head; exit 1; }
echo "зелёный:"; "$OUT/sverka6" | tee "$OUT/sverka6_green.txt" | grep -E "CHECK|RESULT" | cut -c1-260; green=${PIPESTATUS[0]}
[ "$nred" -ge "$ntot" ] && [ "$green" = 0 ] && echo "PASS (красный поймал все $ntot, зелёный сошёлся)" || { echo "FAIL (красный упал $nred из $ntot, зелёный код $green)"; exit 1; }
