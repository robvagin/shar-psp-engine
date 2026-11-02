#!/bin/bash
# №1132: группы пешеходов по зонам на данных всех 7 уровней (хост, без эмулятора). Прежнее (PEDZONE1132_OLD=1) — красный контроль, обязан упасть.
#   tests/pedzone/run.sh [STAGE]   (STAGE по умолчанию ~/Downloads/shar-psp/build/ship_stage)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
STAGE="${1:-$HOME/Downloads/shar-psp/build/ship_stage}"
OUT=${TMPDIR:-/tmp}/pedzone.$$; mkdir -p "$OUT"
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
$CC -DPEDZONE1132_OLD=1 tests/pedzone/host.c $BOTS_LIB -lm -o "$OUT/red" || { echo "FAIL: сборка (красный)"; exit 1; }
$CC tests/pedzone/host.c $BOTS_LIB -lm -o "$OUT/green" || { echo "FAIL: сборка"; exit 1; }
green=0; red=0
for L in 1 2 3 4 5 6 7; do
  if [ $L = 1 ]; then P="$STAGE"; else P="$STAGE/l$L"; fi
  A="$P/level.shr $STAGE/homer.shr $P/level0$L.msn"
  BW_NO_ROOMS=1 BW_NO_COLLECT=1 BW_NO_AUDIO=1 "$OUT/green" $A > "$OUT/g$L.txt" 2>&1 || { green=1; echo "зелёный L$L упал:"; head -5 "$OUT/g$L.txt"; }
  grep -h "^L$L:" "$OUT/g$L.txt"
  if BW_NO_ROOMS=1 BW_NO_COLLECT=1 BW_NO_AUDIO=1 "$OUT/red" $A > "$OUT/r$L.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН L$L"; red=1; else echo "красный L$L поймал: $(grep -m1 FAIL "$OUT/r$L.txt" | cut -c1-120)"; fi
done
rm -rf "$OUT"; [ $green = 0 ] && [ $red = 0 ] && echo "PASS" || { echo FAIL; exit 1; }
