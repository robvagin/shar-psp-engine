#!/bin/bash
# A1.4: tests/barrel_a14/run.sh [level07.msn]. Новое зелёное; красный контроль -DMSN_BARRELLOST_OLD=1 (сигнал «бочка разбита» от хоста не шлётся) обязан упасть
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/barrel_a14}; mkdir -p "$O"
M=${1:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15/l7/level07.msn}
SRC="tests/barrel_a14/host.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
B="cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission"
$B $SRC -lm -o "$O/new" || exit 1; $B -DMSN_BARRELLOST_OLD=1 $SRC -lm -o "$O/old" || exit 1
"$O/new" "$M" | tail -5; POS=${PIPESTATUS[0]}
"$O/old" "$M" | tail -2; "$O/old" "$M" >/dev/null && { echo "красный контроль MSN_BARRELLOST_OLD НЕ упал"; rm -rf "$O"; exit 1; }
rm -rf "$O"; [ "$POS" = 0 ] && echo PASS || { echo FAIL; exit 1; }
