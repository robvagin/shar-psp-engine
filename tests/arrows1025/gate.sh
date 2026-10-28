#!/bin/bash
# Гейт №1025: стрелки пути по поверхности дороги (tests/arrows1025/gate.c), L1, L4, L7 данных тура: новое — 0 % утонувших углов и висящих ≤ 2 %, прежнее хуже; красный (--flat: без наклона к нормали) должен быть пойман. Последняя строка PASS/FAIL
set -u; cd "$(dirname "$0")/../.."; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
D=${DATA:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}; mkdir -p build/ar1025/data; rc=0
bash -c 'V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I viewer -I tools/bots tests/arrows1025/gate.c $BOTS_LIB -lm -o build/ar1025/gate' || { echo "не собрался"; echo FAIL; exit 1; }
for L in 1 4 7; do
  if [ $L = 1 ]; then S=$D; LV=$D/level.shr; HM=$D/homer.shr; else S=$D/l$L; LV=$S/level.shr; HM=$S/player.shr; fi
  TR=$S/traffic.shr; [ -f "$TR" ] || TR=$D/traffic.shr
  ln -sf "$LV" build/ar1025/data/level.shr; ln -sf "$HM" build/ar1025/data/homer.shr; ln -sf "$TR" build/ar1025/data/traffic.shr
  echo "L$L:"; ( cd build/ar1025/data; BW_NO_ROOMS=1 BW_NO_AUDIO=1 ../gate level.shr homer.shr ) | sed 's/^/  /'; [ ${PIPESTATUS[0]} = 0 ] || rc=1
  ( cd build/ar1025/data; BW_NO_ROOMS=1 BW_NO_AUDIO=1 ../gate level.shr homer.shr --flat ) | tail -3 | sed 's/^/  красный: /'; [ ${PIPESTATUS[0]} = 0 ] || rc=1
done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
