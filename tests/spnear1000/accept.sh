#!/bin/bash
# №1000 приёмка (сон далёких стейтпропов-деревьев): хост-гейт gate.c на уровнях L3–L7 (A: все шагают, B: далёкие спят; побитно), красный контроль -DSPNEAR1000_RED (сон без догона времени)
# и dt, меняющийся по кадрам (допуск 1 мс). Последняя строка PASS/FAIL. SET — набор данных тура (по умолчанию test71tour).
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; OUT=${OUT:-$W/build/s1000}; mkdir -p $OUT; rc=0
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}; FR=${FR:-1800}
SRC="$H/gate.c $W/viewer/col.c $W/viewer/props.c $W/viewer/stateprops.c $W/viewer/car.c $W/viewer/coins.c"
cc -std=c11 -O2 -w -DSKIN_HOST -I $W/viewer $SRC -lm -o $OUT/g_new || { echo "не собрался"; echo FAIL; exit 1; }
cc -std=c11 -O2 -w -DSKIN_HOST -DSPNEAR1000_RED -I $W/viewer $SRC -lm -o $OUT/g_red || exit 1
for L in 3 4 5 6 7; do
  echo "L$L зелёный:  $($OUT/g_new $SET/l$L/level.shr $FR | tr '\n' ' ')"; $OUT/g_new $SET/l$L/level.shr $FR >/dev/null || rc=1
  echo "L$L dt меняется: $(MIXDT=1 $OUT/g_new $SET/l$L/level.shr $FR | tr '\n' ' ')"; MIXDT=1 $OUT/g_new $SET/l$L/level.shr $FR >/dev/null || rc=1
  $OUT/g_red $SET/l$L/level.shr $FR >/dev/null && { echo "L$L КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; } || echo "L$L красный: упал, как должно быть"
done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
