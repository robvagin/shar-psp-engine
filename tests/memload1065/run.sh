#!/bin/bash
# №1065: tests/memload1065/run.sh ИМЯ СЦЕНАРИЙ EBOOT — один прогон; итог $OUT/out_ИМЯ/smoke.txt
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test77tour/PSP/GAME/SHARTEST15} OUT=${OUT:-$W/build/ml1065/run}; mkdir -p $OUT
export EMU_OWNER=${EMU_OWNER:-Оптимизация} EMU_MINE=${EMU_MINE:-3}
bash $W/tools/emu_run.sh "$2" "$1" "$3" ${MAXT:-900}
