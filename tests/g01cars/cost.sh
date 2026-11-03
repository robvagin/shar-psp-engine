#!/bin/bash
# сверка g01-cars №1159: счётчики обходов физики машин на кадр, версия кода REF (по умолчанию HEAD) против базы BASE (по умолчанию integ/test83)
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
#   tests/g01cars/cost.sh [SECS] [SEED] [DTMODE]   → build/g1159/<вид>.txt ; вид: base (код TEST83), cur (рабочая копия), cur_old (рабочая копия с -DSV_CARCOST1159_OLD=1)
cd "$(dirname "$0")/../.."; D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data/l5}; SECS=${1:-120}; SEED=${2:-1}; MODE=${3:-1}
export PILOT_AT=${PILOT_AT:--154.9,-482.0,60,0.0238}
BASE=${BASE:-integ/test83}; O=build/g1159; rm -rf $O; mkdir -p $O/base $O/cur
git archive "$BASE" viewer | tar -x -C $O/base; rsync -a --exclude 'build*' viewer $O/cur/
for k in base cur; do python3 tests/g01cars/cost_prep.py $O/$k/viewer || exit 1; done
for k in base cur cur_old; do
  src=$k; EXTRA=""; [ $k = cur_old ] && { src=cur; EXTRA="-DSV_CARCOST1159_OLD=1"; }
  V=$O/$src/viewer; . tools/bots/lib.sh
  cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h -DBW_DT=bwDtNow $EXTRA tests/g01cars/cost.c $BOTS_LIB -lm -o $O/cost_$k 2> $O/cc_$k.txt || { echo "сборка $k: ошибка"; head -20 $O/cc_$k.txt; exit 1; }
done
for k in base cur cur_old; do $O/cost_$k $D/level.shr $D/player.shr $D/level05.msn $SECS $SEED $MODE > $O/$k.txt 2>&1 & done; wait
for k in base cur cur_old; do echo "== $k"; cat $O/$k.txt; done
