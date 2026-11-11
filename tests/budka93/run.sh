#!/bin/bash
# Будка-93: tests/budka93/run.sh ИМЯ СЦЕНАРИЙ [EBOOT] — безголовый прогон; итог $OUT/out_ИМЯ/smoke.txt
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15} OUT=${OUT:-$W/build/b93/run} SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7"
export EMU_OWNER=${EMU_OWNER:-Budka93} EMU_MINE=${EMU_MINE:-1}; mkdir -p $OUT
[ -n "${OVL:-}" ] || { mkdir -p $OUT/ovl_empty; export OVL=$OUT/ovl_empty; }
nice -n 20 bash $W/tools/emu_run.sh "$2" "$1" "${3:-$W/viewer/build_b93/EBOOT.PBP}" ${MAXT:-900}
