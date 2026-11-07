#!/bin/bash
# №1201 (g01-cars): куст-стейтпроп не тормозит машину в полёте. tests/jumps/twin.c на настоящем level.shr: jz4_k100 и jz4_gas (L1 F12-6, предмет типа 78 на башне) должны быть в допуске; красный контроль -DBUSHAIR1201_OLD=1 -DBUSHCLS1201_OLD=1 (как в TEST85…90) обязан падать.
#   tests/g01cars/bushair1201.sh [level.shr]
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
LV=${1:-${LVL1:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15/level.shr}}; mkdir -p build/g1201; rc=0
for k in new old; do F=""; [ $k = old ] && F="-DBUSHAIR1201_OLD=1 -DBUSHCLS1201_OLD=1"
  cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $F tests/jumps/twin.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o build/g1201/twin_$k || exit 1
  build/g1201/twin_$k "$LV" > build/g1201/out_$k.txt 2>&1; bad=$(grep -a -E '^jz4_(k100|gas)' build/g1201/out_$k.txt | grep -c 'НЕТ')
  echo "== $k: jz4 вне допуска: $bad"; grep -a -E '^jz4_(k100|gas)' build/g1201/out_$k.txt | cut -c1-150
  if [ $k = new ]; then [ "$bad" = 0 ] && echo "PASS: куст в воздухе не тормозит" || { echo "FAIL: новое"; rc=1; }; else [ "$bad" != 0 ] && echo "красный контроль пойман" || { echo "FAIL: красный не пойман"; rc=1; }; fi
done; exit $rc
