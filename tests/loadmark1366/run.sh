#!/bin/bash
# 1366: знак загрузки только на закрытом ирисе. tests/loadmark1366/run.sh EBOOT [ПАПКА_с_loadmark.bin]  (loadmark.bin: python3 tools/loadmark_export.py ФАЙЛ 30). Эмулятор безголовый, по одному, nice 20.
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}
OUT=${OUT:-build/loadmark1366}; mkdir -p $OUT; OUT=$(cd $OUT && pwd); OVL=${2:-$OUT/ovl}
if [ ! -f $OVL/loadmark.bin ]; then mkdir -p $OVL; python3 tools/loadmark_export.py $OVL/loadmark.bin 30 || exit 2; fi
export SET OVL OUT SMOKE_HEADLESS=1 EMU_MINE=${EMU_MINE:-1}
# 1) ирис, закрытый надолго: новое и OLD (сценарии tests/loadmark1360)
for O in 0 1; do N=lm1360_$O; F=$OUT/$N.scn
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "wait 15"; echo "flag player 1"; echo "wait 2"
    [ "$O" = 1 ] && echo "flag loadmark1360old 1"
    echo "flag irisslow 12"; echo "flag iristest"; echo "wait 0.6"
    for i in $(seq 0 13); do echo "wait 0.1"; echo "shot m$i"; done; echo "end"; } > $F
  nice -n 20 bash $RUN $F $N $EB 400 > $OUT/$N.log 2>&1; echo "run $N: $(tail -1 $OUT/$N.log)"; done
# 2) экраны вокруг игры (старт, меню, газета, загрузка): новое, OLD (файл loadmark1360old: флаги сценария во фронтенде не читаются) и красный контроль (файл loadmark1366old = поведение 1360)
for O in new old red; do
  S=$OUT/sn_$O.txt; sed 's/^flag loadshot 1.6$/flag newsshot 1\nflag loadshot 1.6/' tools/screens_scenario.txt > $S
  OV=$OUT/ovl_$O; rm -rf $OV; mkdir -p $OV; cp $OVL/loadmark.bin $OV/loadmark.bin
  [ $O = old ] && : > $OV/loadmark1360old; [ $O = red ] && : > $OV/loadmark1366old
  OVL=$OV nice -n 20 bash $RUN $S sn$O $EB 200 > $OUT/sn$O.log 2>&1; echo "run sn$O: $(tail -1 $OUT/sn$O.log)"; done
for O in new old; do rm -rf $OUT/out_sc$O; cp -R $OUT/out_sn$O $OUT/out_sc$O; done
python3 tests/loadmark1366/check.py $OUT
