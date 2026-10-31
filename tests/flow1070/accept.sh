#!/bin/bash
# №1070–1076 приёмка одной командой: зелёный EBOOT (viewer/build_f81tour) и красный (-DMS_FLOW1070_OLD=1 -DCAR_NEAR1073_OLD=1 -DNPC_WAIT1076_OLD=1,
# viewer/build_f81red) гоняют tests/flow1070/r2.txt на наборе тура TEST80; зелёный обязан пройти check.py new, красный — check.py old (иначе гейт ничего не доказывает).
# OUT=папка (по умолчанию build/flow1070); SET — набор (по умолчанию тур TEST80). Последняя строка: PASS или FAIL
set -u
W=$(cd "$(dirname "$0")/../.." && pwd); OUT=${OUT:-$W/build/flow1070}; mkdir -p "$OUT"
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test80tour/PSP/GAME/SHARTEST15}; export SET OUT FLAGS=sfxlog EMU_OWNER=flow1070
"$W/tools/emu_run.sh" "$W/tests/flow1070/r2.txt" new "$W/viewer/build_f81tour/EBOOT.PBP" 500 > "$OUT/new.log" 2>&1 &
"$W/tools/emu_run.sh" "$W/tests/flow1070/r2.txt" old "$W/viewer/build_f81red/EBOOT.PBP" 500 > "$OUT/old.log" 2>&1 &
wait
bad=0
echo "new:"; python3 "$W/tests/flow1070/check.py" new "$OUT/out_new/smoke.txt" || bad=1
echo "old (красный):"; python3 "$W/tests/flow1070/check.py" old "$OUT/out_old/smoke.txt" || { echo "  !! красный не краснеет"; bad=1; }
[ $bad -eq 0 ] && echo PASS || echo FAIL
