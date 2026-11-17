#!/bin/bash
# Правка Ф2-4/A2.3 (Стена 107): стоящая машина этапа без ИИ (L1 m3 этап 1.2, smith_v) урона от тарана не получает; красный -DMC_PARKED_INVULN_OLD=1 (урон растёт).
# Использование: tests/stena107/ram_gate.sh [level.shr homer.shr level01.msn]  (по умолчанию данные TEST106)
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/ram_parked.c $BOTS_LIB -lm -o "$OUT/$NAME" && "$OUT/$NAME" "$L" "$H" "$M" | tail -1 > "$OUT/$NAME.txt"; }
NAME=ram_green; b; NAME=ram_old; b -DMC_PARKED_INVULN_OLD=1
g() { sed -n 's/.*damage_max \([0-9.]*\).*/\1/p' "$OUT/$1.txt"; }
echo "зелёный: урон $(g ram_green); старое: урон $(g ram_old)"
awk -v a="$(g ram_green)" -v b="$(g ram_old)" 'BEGIN{ if (a+0 == 0 && b+0 > 0.05) { print "PASS"; exit 0 } print "FAIL"; exit 1 }'
