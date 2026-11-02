#!/bin/bash
# Окно g07-missions, круг 6: строки game_msn.c на настоящем мире ботов (tools/bots/world.c, настоящие данные диска). Зелёный — как в оригинале;
# красный контроль — все выключатели *_OLD=1 игры: КАЖДАЯ проверка, прошедшая в зелёном, обязана упасть в красном (имена *_info — справочные, не в счёте).
#   tests/g07_sverka/host_game.sh      -> PASS/FAIL, build/g07_sverka/hg_*.txt
# Данные: build/level.shr (L1), build/levels/lN/level.shr (L2–L7), build/missions/level0N.msn, build/homer.shr
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
export V=viewer; . tools/bots/lib.sh
O=${OUT_DIR:-build/g07_sverka}; mkdir -p "$O"
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
OLD="-DGAME_UFOBEAM_OLD=1 -DGAME_UFOCENTER_OLD=1 -DGAME_SAFE13_OLD=1 -DGAME_C3_SAFE_OLD=1 -DGAME_SHOPPHASE_OLD=1 -DGAME_ABORT_OLD=1 -DMSN_ABORT_OLD=1 -DGAME_COUNT57_OLD=1 -DGAME_TALKRET_OLD=1 -DGAME_HIDECHAR_OLD=1 -DGAME_C5_OLD=1 -DMSN_DUMPHIT_OLD=1 -DMSNWALK1113_OLD=1 -DMSNLOOP1113_OLD=1 -DGAME_C7_TALKHIDE_OLD=1 -DGAME_C7_NPCBACK_OLD=1"
$CC tests/g07_sverka/host_game.c $BOTS_LIB -lm -o "$O/host_game" 2> "$O/hg_build.txt" || { echo "FAIL: сборка"; head -5 "$O/hg_build.txt"; exit 1; }
$CC $OLD tests/g07_sverka/host_game.c $BOTS_LIB -lm -o "$O/host_game_old" 2>> "$O/hg_build.txt" || { echo "FAIL: сборка красного"; exit 1; }
lvl() { if [ "$1" = 1 ]; then echo build/level.shr; else echo build/levels/l$1/level.shr; fi; }
POS=0; : > "$O/hg_green_all.txt"; : > "$O/hg_red_all.txt"
for N in ${LEVELS:-1 2 6 7}; do
  printf -v MSN 'build/missions/level%02d.msn' "$N"
  "$O/host_game" "$(lvl $N)" build/homer.shr "$MSN" > "$O/hg_green_$N.txt" 2>&1 || POS=1
  "$O/host_game_old" "$(lvl $N)" build/homer.shr "$MSN" > "$O/hg_red_$N.txt" 2>&1
  sed "s/^CHECK /L$N /" "$O/hg_green_$N.txt" | grep '^L' >> "$O/hg_green_all.txt"; sed "s/^CHECK /L$N /" "$O/hg_red_$N.txt" | grep '^L' >> "$O/hg_red_all.txt"
  echo "== L$N (как в оригинале): $(grep -c 'OK  ' "$O/hg_green_$N.txt") OK, $(grep -c ' FAIL ' "$O/hg_green_$N.txt") FAIL, $(grep -c SKIP "$O/hg_green_$N.txt") SKIP"
  grep ' FAIL ' "$O/hg_green_$N.txt" | head -5
done
# красный: у каждой зелёной (без _info) проверки та же в красном обязана быть FAIL
MISS=0
while read -r L NAME REST; do
  case "$NAME" in *_info) continue;; esac
  grep -q "^$L $NAME .*FAIL" "$O/hg_red_all.txt" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $L $NAME"; MISS=$((MISS+1)); }
done < <(grep ' OK  ' "$O/hg_green_all.txt")
GREENS=$(grep -c ' OK  ' "$O/hg_green_all.txt"); REDS=$(grep -c ' FAIL ' "$O/hg_red_all.txt")
echo "== прежнее поведение (красный): проверок зелёных $GREENS, в красном упало $REDS, не поймано $MISS"
if [ $POS = 0 ] && [ $MISS = 0 ] && [ "$GREENS" -gt 0 ]; then echo PASS; else echo "FAIL (зелёный код $POS, красных не поймано $MISS)"; exit 1; fi
