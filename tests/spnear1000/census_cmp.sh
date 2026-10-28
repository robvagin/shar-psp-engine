#!/bin/bash
# №1000: перепись столкновений L1–L7 до/после (tests/lamp975/census.c, режим S: заезд в центр объёма каждого стейтпропа на 8 курсах): вывод побитно одинаков. До — viewer/ из коммита ОСНОВА (по умолчанию 2247009b).
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; OUT=${OUT:-$W/build/s1000}; BASE=${BASE:-2247009b}; mkdir -p $OUT/old; rc=0
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
(cd $W && git archive $BASE viewer | tar -x -C $OUT/old)
for V in old new; do VD=$OUT/old/viewer; [ $V = new ] && VD=$W/viewer
  cc -std=c11 -O2 -w -DSKIN_HOST -I $VD $H/../lamp975/census.c $VD/col.c $VD/props.c $VD/stateprops.c $VD/car.c $VD/coins.c -lm -o $OUT/cen_$V || { echo "не собрался $V"; echo FAIL; exit 1; }; done
for L in 1 2 3 4 5 6 7; do F=$SET/l$L/level.shr; [ $L = 1 ] && F=$SET/level.shr
  $OUT/cen_old $F S > $OUT/cen_old_L$L.txt 2>&1; $OUT/cen_new $F S > $OUT/cen_new_L$L.txt 2>&1
  if cmp -s $OUT/cen_old_L$L.txt $OUT/cen_new_L$L.txt; then echo "  L$L: перепись совпала ($(tail -1 $OUT/cen_new_L$L.txt | cut -c1-110))"; else echo "  L$L: РАЗНЫЕ"; rc=1; fi; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
