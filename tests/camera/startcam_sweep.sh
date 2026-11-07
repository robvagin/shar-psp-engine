#!/bin/bash
# Аудит 04.11: камеры старта всех этапов миссий всех уровней на настоящем пути игрока (flag player 1, MISSION SELECT k).
#   tests/camera/startcam_sweep.sh НАБОР [уровни] [номера_миссий]     итог: build/sweep/result.txt (одна строка на этап)
# На этап: играла ли камера старта, конец облёта, ближайший подход глаза к голове героя за 1,5 с после конца (apu1170_blend.py), снимки х3
set -u; cd "$(dirname "$0")/../.."
SRC="${1:?набор}"; LV="${2:-1 2 3 4 5 6 7}"; KS="${3:-0 1 2 3 4 5 6}"; export SHIP_LEVELS="2 3 4 5 6 7"
OUT=${SWEEP_OUT:-build/sweep}; mkdir -p $OUT; : > $OUT/result.txt
for L in $LV; do for K in $KS; do
  S=$OUT/s_${L}_$K.txt
  { echo "scene SW_${L}_$K"; echo "flag camdet 1"; echo "flag camlog"; echo "flag startcam 1"
    [ "$L" != 1 ] && echo "flag golevel $L"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnsel $K"
    echo "wait 1.5"; echo "shot a"; echo "wait 2"; echo "shot b"; echo "wait 2.5"; echo "shot c"; echo "wait 2"; echo "end"; } > $S
  SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$S" SMOKE_MAXT=300 tools/smoke.sh "$SRC" > $OUT/l_${L}_$K.log 2>&1
  D=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' $OUT/l_${L}_$K.log)
  if [ -z "$D" ]; then echo "L$L k$K: нет отчёта" >> $OUT/result.txt; continue; fi
  R=$(python3 tests/camera/apu1170_blend.py "$D" | sed 's/^[^:]*: //'); echo "L$L k$K [$(basename $D)]: $R" >> $OUT/result.txt
done; done
echo ALLDONE >> $OUT/result.txt
