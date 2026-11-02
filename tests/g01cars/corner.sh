#!/bin/bash
# сверка g01-cars №1126: сборка и прогон corner.c (повороты на скорости по точкам хода автопилота L5)
#   tests/g01cars/corner.sh ИМЯ [флаги cc...]   → build/g1126/corner_ИМЯ, печать итога; точки хода — build/g1126/poses.txt (делает streets при STREETS_POSES)
cd "$(dirname "$0")/../.."; V=viewer; . tools/bots/lib.sh
N=$1; shift
D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data/l5}
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h -DBW_DT=bwDtNow"
if [ ! -f build/g1126/poses.txt ]; then
  $CC tests/g01cars/streets.c $BOTS_LIB -lm -o build/g1126/streets_p || exit 1
  PILOT_AT=${PILOT_AT:--154.9,-482.0,60,0.0238} STREETS_POSES=build/g1126/poses.txt build/g1126/streets_p $D/level.shr $D/player.shr $D/level05.msn 400 1 0 >/dev/null
fi
$CC "$@" tests/g01cars/corner.c $BOTS_LIB -lm -o build/g1126/corner_$N || exit 1
build/g1126/corner_$N $D/level.shr $D/player.shr $D/level05.msn build/g1126/poses.txt > build/g1126/corner_$N.txt
echo "== $N"; cat build/g1126/corner_$N.txt
