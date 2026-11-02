#!/bin/bash
# сверка g01-cars №1126: сборка замерщика streets в двух видах (новый код / все выключатели сверки =1) и прогон по L5
#   tests/g01cars/streets.sh [SECS] [SEEDS] [DTMODES]   → build/g1126/<вид>_<seed>_<dtmode>.txt
cd "$(dirname "$0")/../.."; V=viewer; . tools/bots/lib.sh
D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data/l5}; SECS=${1:-600}; SEEDS=${2:-"1 2 3"}; MODES=${3:-"0 1 2"}
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h"
mkdir -p build/g1126
ALL=$(grep -ho "SV_[A-Z0-9_]*_OLD" $V/car.c $V/carvis.c $V/skid.c | sort -u | sed 's/^/-D/;s/$/=1/' | tr '\n' ' ')
# BW_DT берёт шаг из замерщика
$CC -DBW_DT=bwDtNow tests/g01cars/streets.c $BOTS_LIB -lm -o build/g1126/streets_new || exit 1
$CC -DBW_DT=bwDtNow $ALL ${EXTRA_OLD:-} tests/g01cars/streets.c $BOTS_LIB -lm -o build/g1126/streets_old || exit 1
$CC -DBW_DT=bwDtNow -DFLIPLAG1126_OLD=1 tests/g01cars/streets.c $BOTS_LIB -lm -o build/g1126/streets_fl0 || exit 1   # новый код, но прежний счёт подшагов
for k in ${VARIANTS:-new old fl0}; do for s in $SEEDS; do for m in $MODES; do
  build/g1126/streets_$k $D/level.shr $D/player.shr $D/level05.msn $SECS $s $m > build/g1126/${k}_${s}_${m}.txt 2>&1 &
done; done; done; wait
for k in ${VARIANTS:-new old fl0}; do for s in $SEEDS; do for m in $MODES; do echo "== $k seed $s dt $m"; cat build/g1126/${k}_${s}_${m}.txt; done; done; done
