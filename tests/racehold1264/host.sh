#!/bin/bash
# Хост-гейт 1264: tests/racehold1264/host.sh [level.shr traffic.shr]. Красный -DTRF_RACEHOLD1264_OLD=1 обязан упасть на (1), зелёный пройти.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
LVL=${1:-$HOME/Downloads/shar-psp/build/level.shr}; TRF=${2:-$HOME/Downloads/shar-psp/build/traffic.shr}; OUT=build/rh1264h; mkdir -p $OUT
b() { cc -std=c11 -O2 -w -DSKIN_HOST -DCOL_DEBUG -DTRAFFIC_ZONE_PARK=1 "${@:2}" -I viewer tests/racehold1264/host.c viewer/traffic.c viewer/skin.c viewer/col.c viewer/car.c viewer/watchdog.c -lm -o $OUT/$1 2> $OUT/$1.err || { echo "сборка $1 не прошла"; tail -5 $OUT/$1.err; exit 2; }; }
b green; b red -DTRF_RACEHOLD1264_OLD=1
$OUT/green "$LVL" "$TRF" > $OUT/green.txt 2>&1; gr=$?
$OUT/red "$LVL" "$TRF" > $OUT/red.txt 2>&1; rd=$?
echo "--- зелёный"; cat $OUT/green.txt; echo "--- красный"; cat $OUT/red.txt
[ $gr = 0 ] && [ $rd = 1 ] && grep -q 'FAIL (1)\|FAIL .*(1)' $OUT/red.txt && echo "ГЕЙТ ЗЕЛЁНЫЙ (красный пойман)" || { echo "ГЕЙТ КРАСНЫЙ gr=$gr rd=$rd"; exit 1; }
