#!/bin/bash
# Правка Ф2-11 (Стена 107): на отсчёте «3 2 1» гонки (L1 sr1) кнопка сброса машины не работает; красный -DMSN_RESETCOUNT_OLD=1 (машину переставляет).
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/reset_count.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=rc_green; b; NAME=rc_old; b -DMSN_RESETCOUNT_OLD=1
g=$("$OUT/rc_green" "$L" "$H" "$M" | sed -n 's/RESULT moved=//p'); o=$("$OUT/rc_old" "$L" "$H" "$M" | sed -n 's/RESULT moved=//p')
echo "перестановка машины на отсчёте: новый $g м, старое $o м"; awk -v a="$g" -v b="$o" 'BEGIN{exit !(a+0 < 0.1 && b+0 > 1)}' && { echo PASS; exit 0; }; echo FAIL; exit 1
