#!/bin/bash
# Гейт Коллизии-96 «стоящие машины сажаются на землю при включении места, а не все сразу при сбросе трафика» (viewer/traffic.c TRF_LAZYPARK_OLD, viewer/car.c carInitDeferred).
# Мак, CPU. Хост tests/parkzone/host.c на build/level.shr + build/traffic.shr; проверки (5) и (6) из его вывода (проверки (2)(3) прежнего гейта устарели с кругом 5 и здесь не читаются).
# Красный: -DTRF_LAZYPARK_OLD=1 (сброс опрашивает землю у каждого места) обязан упасть на (5). Зелёный: ok (5) и ok (6).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
LVL=${1:-build/level.shr}; TRF=${2:-build/traffic.shr}; OUT=build/parkzone; mkdir -p "$OUT"
build() { cc -std=c11 -O2 -w -DSKIN_HOST -DCOL_DEBUG -DTRAFFIC_ZONE_PARK=1 "$@" -I viewer tests/parkzone/host.c viewer/traffic.c viewer/skin.c viewer/col.c viewer/car.c viewer/watchdog.c -lm -o "$OUT/lazy$1" 2> "$OUT/lazy_build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/lazy_build.txt"; exit 1; }; }
build -DGREEN; build -DTRF_LAZYPARK_OLD=1
"$OUT/lazy-DTRF_LAZYPARK_OLD=1" "$LVL" "$TRF" > "$OUT/lazy_red.txt" 2>&1
"$OUT/lazy-DGREEN" "$LVL" "$TRF" > "$OUT/lazy_green.txt" 2>&1
grep "опросов земли" "$OUT/lazy_red.txt" "$OUT/lazy_green.txt"
if grep -q "FAIL (5)" "$OUT/lazy_red.txt"; then echo "красный (сброс сажает все места, как до Коллизии-96): упал на (5), как должен"; else echo "КРАСНЫЙ НЕ ПОЙМАН"; echo "FAIL"; exit 1; fi
if grep -q "ok   (5)" "$OUT/lazy_green.txt" && grep -q "ok   (6)" "$OUT/lazy_green.txt"; then echo "PASS: зелёный (5) сброс без опроса земли, (6) включённые места посажены"; exit 0; fi
grep "(5)\|(6)" "$OUT/lazy_green.txt"; echo "FAIL: зелёный"; exit 1
