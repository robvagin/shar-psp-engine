#!/bin/bash
# №1223: Гомер пешком на пружинах L4 (spring1..3), 10 фаз шестерни №39: долетает до цели дуги и не задевает кольцо. Красный контроль: --off (пружины выключены).
#   tests/airbox1223/springfly.sh [DIR с l4/level.shr и homer.shr]   -> RESULT OK (зелёное) и RESULT FAIL при --off (красное)
set -e; cd "$(dirname "$0")/../.."; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh
cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots tools/bots/springfly.c $BOTS_LIB -lm -o "$O/springfly"
echo "зелёное:"; "$O/springfly" "$D/l4/level.shr" "$D/homer.shr" | grep -a "полётов\|RESULT" || true
echo "красное (--off):"; if "$O/springfly" "$D/l4/level.shr" "$D/homer.shr" --off | grep -a "полётов\|RESULT"; then :; fi
