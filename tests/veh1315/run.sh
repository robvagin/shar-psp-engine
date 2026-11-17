#!/bin/bash
# 108 1315 К7: «разбитая машина: ничего не пишут» — журнал показывает, была ли надпись VEHICLE DESTROYED и почему нет. tests/veh1315/run.sh EBOOT [SET] -> PASS/FAIL
# Зелёный: после разбития (flag testboom) в smoke.txt есть строки VEHDESTROYED ev=destroyed и ev=boom (shown=0/1 + why=ok|tutorial-pending|ui-not-ready|no-wrench). Красный: flag vehdestroyed1315 0 — строк нет (выключатель отключает журнал).
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/veh1315}; mkdir -p "$OUT"; rm -rf "$OUT"/out_vn "$OUT"/out_vo
for v in "vn:1" "vo:0"; do n=${v%%:*}; sed "s/FLAGVAL/${v##*:}/" tests/veh1315/scene.txt > "$OUT/$n.txt"; FLAGS="" nice -n 20 tools/emu_run.sh "$OUT/$n.txt" $n "$EB" 200 > "$OUT/$n.log" 2>&1; done
nd=$(grep -c "^VEHDESTROYED t=.* ev=destroyed" "$OUT/out_vn/smoke.txt"); nb=$(grep -c "^VEHDESTROYED t=.* ev=boom shown=[01] why=" "$OUT/out_vn/smoke.txt"); ro=$(grep -c "^VEHDESTROYED" "$OUT/out_vo/smoke.txt")
grep -h "^VEHDESTROYED" "$OUT/out_vn/smoke.txt" | head -6
echo "новое: destroyed=$nd boom=$nb | выключено: строк $ro"
[ "$nd" -ge 1 ] && [ "$nb" -ge 1 ] && [ "$ro" -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
