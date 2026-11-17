#!/bin/bash
# Правка A2.7 (Стена 107): после пройденной миссии (L1 sr2) машина машины гонки не исчезает мгновенно в кадре уборки; красный -DMC_ENDLEAVE_OLD=1.
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/end_leave.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=el_green; b; NAME=el_old; b -DMC_ENDLEAVE_OLD=1
g=$("$OUT/el_green" "$L" "$H" "$M" | tee "$OUT/el_green.txt" | tail -1); o=$("$OUT/el_old" "$L" "$H" "$M" | tail -1); echo "новый: $g | $(head -1 $OUT/el_green.txt)"; echo "старое: $o"
echo "$g" | grep -q "cars_after_end=[1-9]" && echo "$o" | grep -q "cars_after_end=0" && { echo PASS; exit 0; }; echo FAIL; exit 1
