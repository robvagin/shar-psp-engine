#!/bin/bash
# Правка Ф2-6 (Стена 107): машина этапа ставится на точку, трафик в 5 м от неё убран (L1 m3, smith_v на m4_smithers_carstart); красный -DMSN_SPAWNCLEAR_OLD=1 (машина трафика остаётся под ней).
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/spawn_clear.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=sc_green; b; NAME=sc_old; b -DMSN_SPAWNCLEAR_OLD=1
g=$("$OUT/sc_green" "$L" "$H" "$M" | sed -n 's/RESULT overlap=\([0-9]*\)/\1/p'); o=$("$OUT/sc_old" "$L" "$H" "$M" | sed -n 's/RESULT overlap=\([0-9]*\)/\1/p')
echo "в 5 м от точки: новый $g, старое $o"; [ "$g" = 0 ] && [ "$o" -ge 1 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
