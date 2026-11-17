#!/bin/bash
# Правка A2.2 (Стена 107): машина с ИИ уходит после своего этапа (L1 m5, follow → следующий этап не называет cVan): стоит и пропадает, когда игрок далеко. Красный -DMC_STAGEEND_OLD=1.
set -u; cd "$(dirname "$0")/../.."; D="$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15"; L="${1:-$D/level.shr}"; H="${2:-$D/homer.shr}"; M="${3:-$D/level01.msn}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/stageend.c $BOTS_LIB -lm -o "$OUT/$NAME" && "$OUT/$NAME" "$L" "$H" "$M" | grep -E "^(after_follow|later|RESULT)" > "$OUT/$NAME.txt"; }
NAME=se_green; b; NAME=se_old; b -DMC_STAGEEND_OLD=1
echo "зелёный: $(grep RESULT $OUT/se_green.txt) | $(grep after_follow $OUT/se_green.txt)"; echo "старое:  $(grep RESULT $OUT/se_old.txt) | $(grep after_follow $OUT/se_old.txt)"
grep -q "van_gone=1" "$OUT/se_green.txt" && grep -q "van_gone=0" "$OUT/se_old.txt" && grep -q "mode=0 leaveQ=1" "$OUT/se_green.txt" && { echo PASS; exit 0; }; echo FAIL; exit 1
