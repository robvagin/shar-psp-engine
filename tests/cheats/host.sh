#!/bin/bash
# №1104: ввод и эффекты кодов оригинала. Красный контроль: -DCHEATS1104_OLD=1 (кодов нет) обязан упасть; зелёный — пройти.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/cheats; mkdir -p "$OUT"
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots"
ARGS="build/level.shr build/homer.shr build/missions/level01.msn"
$CC -DCHEATS1104_OLD=1 tests/cheats/host.c $BOTS_LIB -lm -o "$OUT/red" 2> "$OUT/build.txt" || { echo "FAIL: сборка красного"; head -5 "$OUT/build.txt"; exit 1; }
"$OUT/red" $ARGS > "$OUT/red.txt" 2>&1; n=$(grep -c 'CHECK.*FAIL' "$OUT/red.txt")
echo "красный CHEATS1104_OLD=1: упало проверок $n из 12 — $(grep 'CHECK.*FAIL' "$OUT/red.txt" | awk '{print $2}' | tr '\n' ' ')"
$CC tests/cheats/host.c $BOTS_LIB -lm -o "$OUT/green" 2>> "$OUT/build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/build.txt"; exit 1; }
echo "зелёный:"; "$OUT/green" $ARGS | grep -E "CHECK|INFO|RESULT"; green=${PIPESTATUS[0]}
# №1119: красный CHEATCAM1119_OLD=1 — полный цикл камер не идёт (только chase и kull)
$CC -DCHEATCAM1119_OLD=1 tests/cheats/host.c $BOTS_LIB -lm -o "$OUT/red_cam" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного камер"; exit 1; }
if "$OUT/red_cam" $ARGS 2>&1 | grep -q 'CHECK cam_cycle .*FAIL'; then echo "красный CHEATCAM1119_OLD=1: упал на cam_cycle, как должен"; else echo "КРАСНЫЙ НЕ ПОЙМАН: CHEATCAM1119_OLD"; exit 1; fi
[ "$n" -ge 9 ] && [ $green = 0 ] && echo "PASS" || { echo "FAIL: красных $n, зелёный rc=$green"; exit 1; }
