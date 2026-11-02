#!/bin/bash
# Окно g07b-msnview, круг 6: правила экранов миссии (viewer/msn_rules.h). Новое — зелёное; каждый _OLD=1 — красный.
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"
B="cc -std=c99 -O1 -Wall -Wextra -Iviewer -Iviewer/mission"
$B tests/g07b_sverka6/rules_host.c -o "$O/rl_new" || exit 1
POS=0; "$O/rl_new" | grep -E "FAIL|OK:|проверок|тип " ; [ ${PIPESTATUS[0]} = 0 ] || POS=1
NEG=1
for f in SV_NAVWHITE_OLD SV_NAVPICK_OLD SV_NAVARROW_OLD SV_RACE_MSTART3_OLD SV_SKIP7_OLD SV_LOADBLINK_OLD; do
  $B -D$f=1 tests/g07b_sverka6/rules_host.c -o "$O/rl_old" || { NEG=0; continue; }
  n=$("$O/rl_old" | grep -c "FAIL  "); if [ "$n" = 0 ]; then echo "красный контроль $f НЕ упал"; NEG=0; else echo "контроль $f: красных проверок $n"; fi
done
if [ $POS = 0 ] && [ $NEG = 1 ]; then echo PASS; else echo "FAIL (новое: $POS, контроль: $NEG)"; exit 1; fi
