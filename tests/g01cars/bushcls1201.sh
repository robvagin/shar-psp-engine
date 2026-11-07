#!/bin/bash
# №1201б (g01-cars): тело куста (№1137) только у кустов (ATC 10); подбираемые ключи и карточки (ATC 0, масса 0) остаются без тела. Красный контроль -DBUSHCLS1201_OLD=1 обязан упасть. Данные: LVDIR или набор TEST88 интегратора
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
D=${LVDIR:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}; mkdir -p build/g1201; rc=0
LV="$D/level.shr $D/l2/level.shr $D/l3/level.shr $D/l4/level.shr $D/l5/level.shr $D/l6/level.shr $D/l7/level.shr"
for k in base new old; do F=""; [ $k = old ] && F="-DBUSHCLS1201_OLD=1"; [ $k = base ] && F="-DBUSHFALL1137_OLD=1"
  cc -std=c11 -O1 -w -DSKIN_HOST -I viewer $F tests/g01cars/bushcls1201.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o build/g1201/bc_$k || exit 1
  echo "== $k"; out=$(build/g1201/bc_$k $LV); echo "$out"; pick=$(echo "$out" | sed 's/.*типов [0-9]*, с телом \([0-9]*\);.*/\1/')
  [ $k = base ] && basepick=$pick
  [ $k = new ] && { [ "$pick" = "$basepick" ] && echo "PASS: подборы (ключи, карточки) без добавленного тела, кусты с телом" || { echo "FAIL: подборам добавлено тело ($pick против $basepick)"; rc=1; }; }
  [ $k = old ] && { [ "$pick" != "$basepick" ] && echo "красный контроль пойман" || { echo "FAIL: красный не пойман"; rc=1; }; }
done; exit $rc
