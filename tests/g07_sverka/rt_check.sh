#!/bin/bash
# Окно g07-missions, сверка 02.11.2026: рантайм миссий на настоящих данных. Красный контроль — все выключатели *_OLD=1 (прежнее поведение) обязаны упасть.
#   tests/g07_sverka/rt_check.sh [LEVEL.msn ...]  -> PASS/FAIL (данные: build/missions/level0N.msn)
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07_sverka}; mkdir -p "$O"
SRC="tests/g07_sverka/rt_check.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
OLDF="-DMSN_STAGEDELAY_OLD=1 -DMSN_LOCKED_OLD=1 -DMSN_PAR_OLD=1 -DMSN_OOV_OLD=1 -DMSN_TIMERPAUSE_OLD=1 -DMSN_ORDER_OLD=1 -DMSN_DAMAGE_OLD=1 -DMSN_FOLLOW_OLD=1 -DMSN_LOSETAIL_OLD=1 -DMSN_GOOUTSIDE_OLD=1 -DMSN_INTERIOR_ENTER_OLD=1 -DMSN_COINS_OLD=1 -DMSN_DUMP_OLD=1 -DMSN_HINT_OLD=1 -DMSN_ABORT_OLD=1 -DMSN_IRIS_OLD=1 -DMSN_LEVELOVER_OLD=1 -DMSN_TALKORDER_OLD=1 -DMSN_C3_OLD=1 -DMSN_BUYCUR_OLD=1"
cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission $SRC -lm -o "$O/rt_new" || exit 1
cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission $OLDF $SRC -lm -o "$O/rt_old" || exit 1
MSNS=${@:-build/missions/level01.msn}
POS=0; NEG=1
for m in $MSNS; do
  echo "== $m (как в оригинале):"; "$O/rt_new" "$m" || POS=1
done
echo "== прежнее поведение (отрицательный контроль, обязан упасть):"; for m in ${MSNS%% *}; do "$O/rt_old" "$m" >/dev/null && NEG=0 || NEG=1; "$O/rt_old" "$m" | grep -c FAIL; done
if [ $POS = 0 ] && [ $NEG = 1 ]; then echo PASS; else echo "FAIL (новое: код $POS, прежнее упало: $NEG)"; exit 1; fi
