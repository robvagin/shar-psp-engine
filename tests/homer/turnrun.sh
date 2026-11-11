#!/bin/bash
# Issue #2 (konsolenschatz): поведение Гомера на бегу против исходника и PC/PS2: торможение, разворот, круг стика, скорость вдоль лица.
#   tests/homer/turnrun.sh [level.shr homer.shr]   -> зелёное: RESULT OK; красные: -DHOMER_TURN_EXP_OLD=1 (линейный поворот 720°/с), -DHOMER_BRAKE_SNAP_OLD=1 (торможение вместо среза до шага)
set -e; cd "$(dirname "$0")/../.."; D=$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15; LV=${1:-$D/level.shr}; HM=${2:-$D/homer.shr}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"; bad=0
cc $CF tests/homer/turnrun.c $BOTS_LIB -lm -o "$O/turnrun"
for hz in 30 60; do r=$("$O/turnrun" "$LV" "$HM" --hz $hz 2>/dev/null | grep -a "^RESULT"); echo "зелёное ${hz} Гц: $r"; [ "$r" = "RESULT OK" ] || bad=1; done
for flag in HOMER_TURN_EXP_OLD HOMER_BRAKE_SNAP_OLD; do cc $CF -D$flag=1 tests/homer/turnrun.c $BOTS_LIB -lm -o "$O/turnrun_red"; r=$("$O/turnrun_red" "$LV" "$HM" 2>/dev/null | grep -a "^RESULT"); echo "красное -D$flag=1: $r"; [ "$r" = "RESULT FAIL" ] || bad=1; done
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
