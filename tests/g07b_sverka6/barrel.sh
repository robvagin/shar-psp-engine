#!/bin/bash
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"
M=${1:-$HOME/Downloads/shar-psp/build/missions/level07.msn}
SRC="tests/g07b_sverka6/barrel_check.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
B="cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission"
$B $SRC -lm -o "$O/br_new" || exit 1; $B -DMSN_BARREL_OLD=1 $SRC -lm -o "$O/br_old" || exit 1
"$O/br_new" "$M" | tail -6; POS=${PIPESTATUS[0]}
"$O/br_old" "$M" >/dev/null && { echo "красный контроль MSN_BARREL_OLD НЕ упал"; exit 1; }
[ "$POS" = 0 ] && echo PASS || { echo FAIL; exit 1; }
