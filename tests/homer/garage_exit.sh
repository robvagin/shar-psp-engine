#!/bin/bash
# №гараж АЭС L1: Гомер пешком проходит выезд в обе стороны. Зелёный — с правилом лесенки (-DHOMER_STAIR_UP=1), красный контроль — без него.
#   tests/homer/garage_exit.sh [level.shr] [homer.shr]
set -u
cd "$(dirname "$0")/../.."
L=${1:-build/level.shr}; Hm=${2:-build/homer.shr}
T=${TMPDIR:-/tmp}/garage_exit.$$
CC="cc -std=c99 -O2 -w -DSKIN_HOST -I viewer tests/homer/garage_exit.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm"
$CC -DHOMER_STAIR_UP=1 -o "$T.g" || exit 2
$CC -o "$T.r" || exit 2
bad=0
"$T.g" "$L" "$Hm" | tail -2 | tr '\n' ' '; [ ${PIPESTATUS[0]} = 0 ] || bad=1; echo
"$T.r" "$L" "$Hm" > /dev/null && { echo "КРАСНЫЙ КОНТРОЛЬ НЕ ПОЙМАН (без правила Гомер тоже вышел)"; bad=1; } || echo "красный контроль (без HOMER_STAIR_UP): встал у уступа, как должен"
rm -f "$T.g" "$T.r"; [ $bad = 0 ] && echo "garage_exit: PASS" || echo "garage_exit: FAIL"; exit $bad
