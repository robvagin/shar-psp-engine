#!/bin/bash
# 1360: знак загрузки вместо надписи LOADING. Гейт по снимкам: закрытый ирис держится долго (flag irisslow), снимки через 0,1 с. Новое: в правом нижнем углу жёлтый кружок с полями как у радара HUD
# (22 справа, 18 снизу), центр на месте (±1 px), размер пульсирует (меньший ≤ 0,9 большего), надпись LOADING левее по центру кружка по высоте (±1,5 px). Старое (LOADMARK1360_OLD=1): знака нет.
#   tests/loadmark1360/run.sh EBOOT [ПАПКА_с_loadmark.bin]   (loadmark.bin: python3 tools/loadmark_export.py ФАЙЛ 32)
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}
OUT=${OUT:-build/loadmark1360}; mkdir -p $OUT; OUT=$(cd $OUT && pwd)
OVL=${2:-$OUT/ovl}; if [ ! -f $OVL/loadmark.bin ]; then mkdir -p $OVL; python3 tools/loadmark_export.py $OVL/loadmark.bin 32 || exit 2; fi
for O in 0 1; do N=lm1360_$O; F=$OUT/$N.scn
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "wait 15"; echo "flag player 1"; echo "wait 2"
    [ "$O" = 1 ] && echo "flag loadmark1360old 1"
    echo "flag irisslow 12"; echo "flag iristest"; echo "wait 0.6"
    for i in $(seq 0 13); do echo "wait 0.1"; echo "shot m$i"; done; echo "end"; } > $F
  SET=$SET OVL=$OVL SMOKE_HEADLESS=1 OUT=$OUT EMU_MINE=${EMU_MINE:-1} bash $RUN $F $N $EB 400 > $OUT/$N.log 2>&1; echo "run $N: $(tail -1 $OUT/$N.log)"
done
python3 tests/loadmark1360/check.py $OUT
