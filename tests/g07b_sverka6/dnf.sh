#!/bin/bash
# Окно g07b-msnview, круг 6 (P17-15): отсчёт 10 с до DNF на настоящих трассах b01, b04, b07 (tests/bonus/dnf_host.c). Прежнее правило (BR_DNF10_OLD=1, 20 с от первого финишёра) обязано упасть.
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"; V=viewer
BNS=${1:-$HOME/Downloads/shar-psp/build/bonus}
SRC="tests/bonus/dnf_host.c $V/bonus.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c"
B="cc -std=c11 -O1 -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"; LNK="-lm -Wl,-U,_uiFlash"
$B $SRC $LNK -o "$O/dnf_new" 2>/dev/null || exit 1
# красный контроль: тест остаётся «новым» (проверяет правило 10 с), а bonus.c собран с прежним правилом (20 с от первого финишёра)
OBJS=""; for f in $SRC; do o="$O/dnf_$(basename $f .c).o"; if [ "$(basename $f)" = bonus.c ]; then $B -DBR_DNF10_OLD=1 -c $f -o $o 2>/dev/null || exit 1; else $B -c $f -o $o 2>/dev/null || exit 1; fi; OBJS="$OBJS $o"; done
cc $OBJS $LNK -o "$O/dnf_old" || exit 1
POS=0; for t in 1 4 7; do "$O/dnf_new" "$BNS/b0$t.bns" || POS=1; done
NEG=1; for t in 1 4 7; do "$O/dnf_old" "$BNS/b0$t.bns" >/dev/null 2>&1 && { echo "красный контроль b0$t НЕ упал"; NEG=0; }; done
[ $POS = 0 ] && [ $NEG = 1 ] && echo PASS || { echo "FAIL (новое $POS, контроль $NEG)"; exit 1; }
