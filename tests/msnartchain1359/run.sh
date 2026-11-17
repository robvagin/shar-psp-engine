#!/bin/bash
# 1359 (оценка Fable п.6б): насос записей миссии (msnartPump) раз в кадр брал одно чтение; теперь после чтения в том же кадре берёт следующее из очереди (предел 8 мс за кадр, 24 мс при скрытом мире).
# Гейт: «flag msnartchain1359 10» ставит в очередь 10 разных небольших записей и замеряет, за сколько кадров насоса и миллисекунд очередь опустеет (строка MSNARTCHAIN1359). Новое против MSNARTCHAIN1359_OLD=1.
#   tests/msnartchain1359/run.sh EBOOT   → build/msnartchain1359/*; итог check.py
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}
OUT=${OUT:-build/msnartchain1359}; mkdir -p $OUT; OUT=$(cd $OUT && pwd)
for O in 0 1; do N=ch1359_$O; F=$OUT/$N.scn
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "wait 15"; echo "flag player 1"; echo "flag msn"; echo "wait 6"
    [ "$O" = 1 ] && echo "flag msnartchain1359old 1"
    echo "flag msnartchain1359 10"; echo "wait 3"; echo "end"; } > $F
  SET=$SET SMOKE_HEADLESS=1 OUT=$OUT EMU_MINE=${EMU_MINE:-1} bash $RUN $F $N $EB 300 > $OUT/$N.log 2>&1; echo "run $N: $(tail -1 $OUT/$N.log)"
done
python3 tests/msnartchain1359/check.py $OUT
