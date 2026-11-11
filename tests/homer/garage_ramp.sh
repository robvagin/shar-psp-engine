#!/bin/bash
# Гомер пешком идёт по гладкому пандусу 18° выезда гаража АЭС L1 вверх и вниз (как PS2); красный контроль -DHOMER_STEP=0.004f обязан упасть
#   tests/homer/garage_ramp.sh [level.shr] [homer.shr]
set -u
cd "$(dirname "$0")/../.."
L=${1:-build/level.shr}; Hm=${2:-build/homer.shr}; T=${TMPDIR:-/tmp}/garage_ramp.$$
CC="cc -std=c99 -O2 -w -DSKIN_HOST -I viewer tests/homer/garage_ramp.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm"
$CC -o "$T.g" || exit 2; $CC -DHOMER_STEP=0.004f -o "$T.r" || exit 2
bad=0; "$T.g" "$L" "$Hm" | tr '\n' ' '; [ ${PIPESTATUS[0]} = 0 ] || bad=1; echo
"$T.r" "$L" "$Hm" > /dev/null && { echo "КРАСНЫЙ КОНТРОЛЬ НЕ ПОЙМАН"; bad=1; } || echo "красный контроль (HOMER_STEP 0,004): не поднялся, как должен"
rm -f "$T.g" "$T.r"; [ $bad = 0 ] && echo "garage_ramp: PASS" || echo "garage_ramp: FAIL"; exit $bad
