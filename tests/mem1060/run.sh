#!/bin/bash
# №1060: tests/mem1060/run.sh ИМЯ L FRAG HOG [EBOOT] [CAR] [MEM] — один прогон; итог OUT/out_ИМЯ/smoke.txt
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74tour/PSP/GAME/SHARTEST15} OUT=${OUT:-$W/build/m1060/run}; mkdir -p $OUT
export EMU_OWNER=${EMU_OWNER:-Оптимизация} EMU_MINE=${EMU_MINE:-3}
N=$1; L=$2; FR=$3; HOG=$4; E=${5:-$W/viewer/build_m1060/EBOOT.PBP}; CAR=${6:-0}; MEM=${7:-1}; FE=${8:-1}
export OVL=${OVL:-$HOME/Downloads/shar-psp-t74-booth1056/build/b1056/acc/o_new}   # пакеты машин №1056 (42 машины на каждом уровне)
python3 $H/gen.py $L $FR $HOG $CAR $MEM $FE > $OUT/s_$N.txt
bash $W/tools/emu_run.sh $OUT/s_$N.txt $N "$E" ${MAXT:-900}
