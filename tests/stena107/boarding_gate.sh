#!/bin/bash
# Правка A2.11 (Стена 107): при постановке игрока в его машину на старте этапа трафик в 10 м от неё снят (L1 m5); красный -DMSN_SPAWNCLEAR_OLD=1.
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/boarding_clear.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=bc_green; b; NAME=bc_old; b -DMSN_SPAWNCLEAR_OLD=1
g=$("$OUT/bc_green" "$L" "$H" "$M" | sed -n 's/RESULT traffic_within_10m=\([0-9]*\).*/\1/p'); o=$("$OUT/bc_old" "$L" "$H" "$M" | sed -n 's/RESULT traffic_within_10m=\([0-9]*\).*/\1/p')
echo "в 10 м от машины игрока на старте: новый $g, старое $o"; [ "$g" = 0 ] && [ "$o" -ge 1 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
