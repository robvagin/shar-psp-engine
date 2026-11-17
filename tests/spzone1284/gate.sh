#!/bin/bash
# 1284: сбитый ящик (стейтпроп) лежит, пока игрок не дальше 400 м; за зоной возвращается. Красный (-DSPZONE1284_OLD=1): возвращается уже на 100 м.   tests/spzone1284/gate.sh
set -u; cd "$(dirname "$0")/../.."; SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; OUT=build/spzone1284; mkdir -p $OUT
SRC=(tests/spzone1284/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c)
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "${SRC[@]}" -lm -o $OUT/g || { echo "FAIL: сборка"; exit 1; }; cc -std=c11 -O2 -w -DSKIN_HOST -DSPZONE1284_OLD=1 -I viewer "${SRC[@]}" -lm -o $OUT/r || exit 1
g() { grep "^$2 " $OUT/$1.txt | cut -d' ' -f2; }; fail=0
for L in 1 2 3 4 5 6 7; do F=$SET/l$L/level.shr; [ $L = 1 ] && F=$SET/level.shr; $OUT/g $F > $OUT/g.txt; $OUT/r $F > $OUT/r.txt
  echo "L$L: предмет $(g g chosen); зелёный лежит на 100/300/450 м: $(g g lies_100)/$(g g lies_300)/$(g g lies_450); красный: $(g r lies_100)/$(g r lies_300)/$(g r lies_450)"
  [ "$(g g chosen)" -ge 0 ] || continue
  [ "$(g g lies_100)" = 1 ] && [ "$(g g lies_300)" = 1 ] && [ "$(g g lies_450)" = 0 ] || { echo "  FAIL зелёный"; fail=1; }
  [ "$(g r lies_100)" = 0 ] || { echo "  КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
done
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
