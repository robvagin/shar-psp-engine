#!/bin/bash
# Слой «персонаж ↔ машина» (сверка g02-traffic, просьба g07): персонаж миссии идёт сквозь стоящую машину (прежнее, PEDCAR_ALL_OLD=1 — красный контроль, обязан упасть);
# новое — стоит 3,0 с у кузова, обходит по углам и доходит. Мир ботов на данных уровня 1 (tests/npc/pedcar.c).  tests/npc/pedcar.sh [level.shr homer.shr level01.msn]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=${TMPDIR:-/tmp}/pedcar.$$; mkdir -p "$OUT"
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
ARGS="${1:-build/level.shr} ${2:-build/homer.shr} ${3:-build/missions/level01.msn}"
$CC -DPEDCAR_ALL_OLD=1 tests/npc/pedcar.c $BOTS_LIB -lm -o "$OUT/red" || { echo "FAIL: сборка (красный)"; exit 1; }
if "$OUT/red" $ARGS > "$OUT/red.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН: прежнее прошло"; red=0; else echo "красный: $(grep -m1 mission "$OUT/red.txt" | cut -c1-200)"; red=1; fi
$CC tests/npc/pedcar.c $BOTS_LIB -lm -o "$OUT/green" || { echo "FAIL: сборка"; exit 1; }
echo "зелёный:"; "$OUT/green" $ARGS > "$OUT/green.txt" 2>&1; green=$?; grep -E "mission|PASS|FAIL" "$OUT/green.txt" | cut -c1-200
rm -rf "$OUT"; [ $red = 1 ] && [ $green = 0 ] && echo "PASS (красный пойман, зелёный сошёлся)" || { echo FAIL; exit 1; }
