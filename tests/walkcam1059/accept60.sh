#!/bin/bash
# №1059 accept60: зелёный — управление пешком при смене камеры как PS2 (допуски в gate.py); красный — flag walkcam1059old 1 (как TEST74).
# Аргумент 1 — EBOOT (сборка с KEEP_SMOKE); SET — набор данных. Раннер tools/emu_run.sh из integ. Последняя строка GATE PASS / GATE FAIL
H=$(cd "$(dirname "$0")" && pwd); E=${1:?EBOOT}; export OUT=${OUT:-$H/out} EMU_OWNER=${EMU_OWNER:-Камера} EMU_MINE=${EMU_MINE:-2}; rc=0; mkdir -p $OUT
RUN=${RUN:-$HOME/Downloads/shar-psp-integ/tools/emu_run.sh}
red() { awk '{print} /^flag camlog/ && !d {print "flag walkcam1059old 1"; d=1}' "$1" > "$2"; }
for s in exit_dirs open_left booth_left mission_left; do red $H/$s.txt $OUT/${s}_red.txt; done
for s in exit_dirs open_left booth_left mission_left; do bash $RUN $H/$s.txt g_$s $E 700 >/dev/null 2>&1 & bash $RUN $OUT/${s}_red.txt r_$s $E 700 >/dev/null 2>&1 & done; wait
echo "== №1059 зелёный"; python3 $H/gate.py $OUT/out_g_exit_dirs/smoke.txt $OUT/out_g_open_left/smoke.txt $OUT/out_g_booth_left/smoke.txt $OUT/out_g_mission_left/smoke.txt || rc=1
echo "== №1059 красный (flag walkcam1059old 1)"; python3 $H/gate.py $OUT/out_r_exit_dirs/smoke.txt $OUT/out_r_open_left/smoke.txt $OUT/out_r_booth_left/smoke.txt $OUT/out_r_mission_left/smoke.txt --expect fail || { echo "КРАСНЫЙ КОНТРОЛЬ НЕ КРАСНЫЙ"; rc=1; }
[ $rc = 0 ] && echo "GATE PASS" || echo "GATE FAIL"; exit $rc
