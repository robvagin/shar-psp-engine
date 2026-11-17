#!/bin/bash
# 109 (в, Fable-2): лестница car-ahead (чтение машин впрок) при ballast 9 не доходит до ступени 3 (выброс клеток города, даль пропадает на смене этапа).
# Новое: строк "WATCHDOG mem who=car-ahead ... step>=3" нет; красный (flag mcrladold109 1 — прежний вызов 9bc6bd9c): есть.   carahead_gate.sh PACK
set -u; cd "$(dirname "$0")/../.."; P=$1
run() { SMOKE_BALLAST=9 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
cnt() { grep -E "WATCHDOG mem who=car-ahead" "$1/smoke.txt" | grep -cE "step=[345]"; }
tot() { grep -c "WATCHDOG mem who=car-ahead" "$1/smoke.txt"; }
Rn=$(run tests/stena109/carahead_new.txt); Ro=$(run tests/stena109/carahead_old.txt)
echo "  новое: строк car-ahead $(tot $Rn), со ступенью 3+ $(cnt $Rn);  прежнее: строк $(tot $Ro), со ступенью 3+ $(cnt $Ro)"
[ "$(cnt $Rn)" = 0 ] && [ "$(cnt $Ro)" -ge 1 ] && echo PASS || { echo FAIL; exit 1; }
