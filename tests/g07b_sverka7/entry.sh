#!/bin/bash
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka7}; mkdir -p "$O"; D=${MSN_DIR:-$HOME/Downloads/shar-psp/build/missions}
SRC="tests/g07b_sverka7/entry_check.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"; B="cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission"
M=""; for n in 1 2 3 4 5 6 7; do M="$M $D/level0$n.msn"; done
$B $SRC -lm -o "$O/en_new" || exit 1; $B -DMSN_ENTRYRESET_OLD=1 $SRC -lm -o "$O/en_old" || exit 1
"$O/en_new" $M | grep -E "FAIL|OK:|coverage"; POS=${PIPESTATUS[0]}
"$O/en_old" $M >/dev/null && { echo "красный контроль НЕ упал"; exit 1; }
[ "$POS" = 0 ] && echo PASS || { echo FAIL; exit 1; }
