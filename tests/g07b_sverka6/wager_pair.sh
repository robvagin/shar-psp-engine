#!/bin/bash
# Окно g07b-msnview, круг 6: пара «карточка ставки ↔ выплата». Новое должно пройти; прежнее (SV_WAGER_ODDS_OLD=1, MSN_WAGERPAY_OLD=1) обязано упасть.
#   tests/g07b_sverka6/wager_pair.sh [levelNN.msn ...]   (данные по умолчанию: ~/Downloads/shar-psp/build/missions/level0N.msn)
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"
D=${MSN_DIR:-$HOME/Downloads/shar-psp/build/missions}
MSNS=${@:-$D/level01.msn $D/level02.msn $D/level03.msn $D/level04.msn $D/level05.msn $D/level06.msn $D/level07.msn}
SRC="tests/g07b_sverka6/wager_pair.c viewer/mission/msn_file.c viewer/mission/msn_run.c viewer/mission/msn_ops.c"
B="cc -std=c99 -O1 -Wall -Wextra -Iviewer/mission -Iviewer"
$B $SRC -lm -o "$O/wp_new" || exit 1
$B -DSV_WAGER_ODDS_OLD=1 $SRC -lm -o "$O/wp_old_card" || exit 1
$B -DMSN_WAGERPAY_OLD=1 $SRC -lm -o "$O/wp_old_run" || exit 1
POS=0; "$O/wp_new" $MSNS | tail -4 ; [ ${PIPESTATUS[0]} = 0 ] || POS=1
NEG=1
"$O/wp_old_card" $MSNS >/dev/null && { echo "красный контроль (карточка прежняя) НЕ упал"; NEG=0; }
"$O/wp_old_run" $MSNS >/dev/null && { echo "красный контроль (рантайм прежний) НЕ упал"; NEG=0; }
if [ $POS = 0 ] && [ $NEG = 1 ]; then echo PASS; else echo "FAIL (новое: $POS, контроль: $NEG)"; exit 1; fi
