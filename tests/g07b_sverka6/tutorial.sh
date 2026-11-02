#!/bin/bash
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"
B="cc -std=c11 -O1 -Wall -Wno-unused-function -DSKIN_HOST -I tests/tutorial/stub -I viewer"
$B tests/g07b_sverka6/tutorial_host.c viewer/tutorial.c -o "$O/tu_new" || exit 1
"$O/tu_new" | tail -10; POS=${PIPESTATUS[0]}; NEG=1
for f in TUT_QORDER_OLD=1 TUT_FOREIGN_OLD=1; do
  $B -D$f tests/g07b_sverka6/tutorial_host.c viewer/tutorial.c -o "$O/tu_old" || { NEG=0; continue; }
  n=$("$O/tu_old" | grep -c "FAIL  "); if [ "$n" = 0 ]; then echo "красный контроль $f НЕ упал"; NEG=0; else echo "контроль $f: красных проверок $n"; fi
done
[ "$POS" = 0 ] && [ $NEG = 1 ] && echo PASS || { echo FAIL; exit 1; }
