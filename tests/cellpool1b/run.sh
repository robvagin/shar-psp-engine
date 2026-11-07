#!/bin/bash
# 1Б: tests/cellpool1b/run.sh СЦЕНАРИЙ ИМЯ РЕЖИМ [EBOOT] — один прогон в эмуляторе; РЕЖИМ: old (cellpoolold), a (классы 16…192), b (мельче).
# Итог $OUT/out_ИМЯ_РЕЖИМ/smoke.txt. Куча как у PSP: ballast (МБ, по умолчанию 8). Набор данных — SET (по умолчанию набор TEST88).
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test88/PSP/GAME/SHARTEST15} OUT=${OUT:-$W/build/cp1b/run}; mkdir -p $OUT
export EMU_OWNER=${EMU_OWNER:-g13-far} EMU_MINE=${EMU_MINE:-3}
BAL=${BALLAST:-8}; case "$3" in old) F="cellpoolold ballast=$BAL";; b) F="cellpoolcls=b ballast=$BAL";; *) F="ballast=$BAL";; esac
FLAGS="$F" bash $W/tools/emu_run.sh "$1" "$2_$3" "${4:-$W/viewer/build_g90/EBOOT.PBP}" ${MAXT:-3000}
