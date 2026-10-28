#!/bin/bash
# №1062 accept60: контур только у героя, NPC, машин, будки и предметов; зелёный — новый код, красный — flag ink1062old 1 (линии окружения как в TEST74).
# Аргумент 1 — EBOOT тура (с INK1062). SET — набор данных (test74tour). Раннер tools/emu_run.sh, ≤ 2 эмулятора. Последняя строка GATE PASS / GATE FAIL
H=$(cd "$(dirname "$0")" && pwd); E=${1:?EBOOT}; export OUT=${OUT:-$H/out} EMU_OWNER=${EMU_OWNER:-Камера} EMU_MINE=2; rc=0; mkdir -p $OUT
RUN=${RUN:-$HOME/Downloads/shar-psp-integ/tools/emu_run.sh}
OUT=$OUT/n bash $RUN $H/gate.txt n $E 900 >/dev/null 2>&1 & OUT=$OUT/o bash $RUN $H/gate_old.txt n $E 900 >/dev/null 2>&1 & wait
OUT=$OUT/r bash $RUN $H/gate_old.txt n $E 900 >/dev/null 2>&1
echo "== №1062 зелёный"; python3 $H/check.py $OUT/n/out_n $OUT/o/out_n || rc=1
echo "== №1062 красный (flag ink1062old 1)"; python3 $H/check.py $OUT/r/out_n $OUT/o/out_n --expect fail || { echo "КРАСНЫЙ КОНТРОЛЬ НЕ КРАСНЫЙ"; rc=1; }
[ $rc = 0 ] && echo "GATE PASS" || echo "GATE FAIL"; exit $rc
