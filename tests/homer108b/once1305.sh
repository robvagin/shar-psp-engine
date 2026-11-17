#!/bin/bash
# Гейт 1305: разовый предмет, разбитый чужой машиной (трафик, погоня, миссия), не пишется в сохранение и возвращается вне района 400 м; разбитый игроком — пишется и не возвращается.
# Красный (-DSPONCEOTHER1305_OLD=1): разбитый чужой — в сохранении и не вернулся.   tests/homer108b/once1305.sh
set -u; cd "$(dirname "$0")/../.."; O=build/h108b; mkdir -p $O; L=${LEVEL:-$HOME/Downloads/shar-psp/build/levels/l2/level.shr}
SRC="tests/homer108b/once1305.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c viewer/savegame.c"
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer"
$CC $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/once_new || { echo FAIL сборка; exit 1; }
$CC -DSPONCEOTHER1305_OLD=1 $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/once_old || { echo FAIL сборка; exit 1; }
fail=0; echo "новое:"; $O/once_new $L || fail=1
echo "красное (SPONCEOTHER1305_OLD=1):"; $O/once_old $L && { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; } || echo "красный пойман"
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
