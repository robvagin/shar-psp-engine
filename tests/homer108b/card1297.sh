#!/bin/bash
# Гейт 1297: карточка из машины (collect.c): точка машины низ + 0,63 м, углы кузова — родной радиус карточки. Зелёный: A — на 3,2 м карточка не берётся ни под ней, ни в 1,5 м сбоку; на 1 м берётся.
# Красный (-DCARD_CORNER108_OLD=1): на 3,2 м под ней берётся. B: счёт карточек, берущихся машиной с земли, по L1–L7 — прежнее и новое.   tests/homer108b/card1297.sh
set -u; cd "$(dirname "$0")/../.."; O=build/h108b; mkdir -p $O; D=${DATA:-$HOME/Downloads/shar-psp/build}
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer"; SRC="tests/homer108b/card1297.c viewer/collect.c viewer/coins.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c"
$CC $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/card_new || { echo FAIL сборка; exit 1; }
$CC -DCARD_CORNER108_OLD=1 $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/card_old || { echo FAIL сборка; exit 1; }
P1=$D/collect-l1.shr; fail=0
echo "новое:"; $O/card_new A $P1 || fail=1
echo "красное (CARD_CORNER108_OLD=1):"; $O/card_old A $P1 && { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; } || echo "красный пойман"
echo "B по уровням (старое / новое):"
for N in 1 2 3 4 5 6 7; do if [ $N = 1 ]; then PK=$D/collect-l1.shr; LV=$D/levels/l1/level.shr; else PK=$D/levels/l$N/collect-l$N.shr; LV=$D/levels/l$N/level.shr; fi
  [ -f "$PK" ] && [ -f "$LV" ] || { echo " L$N нет данных"; continue; }
  echo " $($O/card_old B $PK $LV | tail -1) | $($O/card_new B $PK $LV | tail -1)"; done
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
