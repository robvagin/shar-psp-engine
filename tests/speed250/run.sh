#!/bin/bash
# скорость 250: tests/speed250/run.sh ИМЯ СЦЕНАРИЙ EBOOT [ФЛАГИ] — один прогон в эмуляторе; итог $OUT/out_ИМЯ/smoke.txt
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74tour/PSP/GAME/SHARTEST15} OUT=${OUT:-$W/build/s250/run}; mkdir -p $OUT
export EMU_OWNER=${EMU_OWNER:-Оптимизация} EMU_MINE=${EMU_MINE:-3}
[ -n "${4:-}" ] && export FLAGS="$4"
bash $W/tools/emu_run.sh "$2" "$1" "$3" ${MAXT:-900}
