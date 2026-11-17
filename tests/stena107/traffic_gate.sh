#!/bin/bash
# Правка Ф2-5 (Стена 107): предел трафика этапов L1 m3 и m5 (за рулём, после вступления) держится 2, как у оригинала; красный -DMSN_TRAFFICHOLD_OLD=1 (этап без команды возвращал 5).
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh; fail=0
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/traffic_cap.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=tc_green; b; NAME=tc_old; b -DMSN_TRAFFICHOLD_OLD=1
for m in 3 5; do g=$("$OUT/tc_green" "$L" "$H" "$M" $m | sed -n 's/.*main_max=\([0-9]*\).*/\1/p'); o=$("$OUT/tc_old" "$L" "$H" "$M" $m | sed -n 's/.*main_max=\([0-9]*\).*/\1/p')
  echo "m$m: новый предел $g, старое $o"; [ "$g" = 2 ] && [ "$o" = 5 ] || fail=1; done
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
