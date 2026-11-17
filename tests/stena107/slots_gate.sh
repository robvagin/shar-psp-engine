#!/bin/bash
# Правка A2.13 (Стена 107): при заполненных 6 местах машина этапа получает место (вытесняется полиция Hit & Run / машина в очереди на уход); красный -DMC_SPAWNEVICT_OLD=1 (молча не появляется).
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/slots.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=sl_green; b; NAME=sl_old; b -DMC_SPAWNEVICT_OLD=1
g=$("$OUT/sl_green" "$L" "$H" "$M" | sed -n 's/RESULT spawned=\([0-9]*\).*/\1/p'); o=$("$OUT/sl_old" "$L" "$H" "$M" | sed -n 's/RESULT spawned=\([0-9]*\).*/\1/p')
echo "машина этапа при 6 занятых местах: новый $g, старое $o"; [ "$g" = 1 ] && [ "$o" = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
