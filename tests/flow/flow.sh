#!/bin/bash
# Круг 6 (сверка g10-flow): зелёный прогон + красные контроли по каждому выключателю, который проверяет tests/flow/flow_host.c
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/flow; mkdir -p "$OUT"
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots"
ARGS="build/level.shr build/homer.shr build/missions/level01.msn"
$CC tests/flow/flow_host.c $BOTS_LIB -lm -o "$OUT/flow" 2> "$OUT/build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/build.txt"; exit 1; }
echo "зелёный:"; "$OUT/flow" $ARGS | grep -E "CHECK|RESULT"; green=${PIPESTATUS[0]}
bad=0
for old in "PAD_BOTH_OLD:pad_both" "CARDEAD_STEER_OLD:cardead_steer" "SV_EMPTYPLAYER_OLD:empty_player" "FOOT_DPAD_FULL_OLD:foot_dpad" "CREDITS_BOTH_OLD:credits_rate" "CREDVO_OLD:credits_lines" "IRISLOAD1112_OLD:iris_loading"; do
  m=${old%%:*}; c=${old##*:}
  $CC -D$m=1 tests/flow/flow_host.c $BOTS_LIB -lm -o "$OUT/red_$m" 2>> "$OUT/build.txt" || { echo "FAIL: сборка красного $m"; bad=1; continue; }
  if "$OUT/red_$m" $ARGS 2>&1 | grep -q "CHECK $c .*FAIL"; then echo "красный $m: упал на $c, как должен"; else echo "КРАСНЫЙ НЕ ПОЙМАН: $m ($c)"; bad=1; fi
done
[ $green = 0 ] && [ $bad = 0 ] && echo "PASS" || { echo "FAIL: зелёный rc=$green, красные bad=$bad"; exit 1; }
