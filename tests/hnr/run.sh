#!/bin/bash
# Гейт правки 121 (Hit & Run по правилам исполняемого файла PAL): tests/hnr/rules.c на логике viewer/game.c (Мак, CPU).
# Сначала КРАСНЫЙ — те же проверки на испорченных правилах обязаны упасть: без паузы между ударами машин (HNR_CAR_COOL 0),
# без паузы спада (HNR_DECAY_DELAY 0); потом ЗЕЛЁНЫЙ на настоящих. Итог: build/hnr/*.txt
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/hnr; mkdir -p "$OUT"; V=viewer
. tools/bots/lib.sh; LIB="$BOTS_LIB"
build() { cc -std=c11 -O1 -w -DSKIN_HOST "$@" -I $V -I tools/bots tests/hnr/rules.c $LIB -lm -o "$OUT/rules_$1" 2> "$OUT/build.txt" ||
          { echo "FAIL: сборка"; head -5 "$OUT/build.txt"; exit 1; }; }
red=0
for plant in "-DHNR_CAR_COOL=0.f" "-DHNR_DECAY_DELAY=0.f" "-DHNR_PED10_OLD=1" "-DHNR_CHASE_PAUSE_OLD=1"; do   # аудит 09.10: пешеход +10 вместо +13 PS2
  build "$plant" -DPLANT; "$OUT/rules_$plant" > "$OUT/red_${plant#-D}.txt" 2>&1; rc=$?
  if [ $rc -ne 0 ]; then echo "красный ($plant): упал, как должен — $(grep -m1 FAIL "$OUT/red_${plant#-D}.txt" | cut -c7-60)"; red=$((red + 1))
  else echo "КРАСНЫЙ НЕ ПОЙМАН: $plant прошёл гейт"; fi
done
build -DGREEN; "$OUT/rules_-DGREEN" > "$OUT/green.txt" 2>&1; rc=$?
grep -c "OK" "$OUT/green.txt" >/dev/null; grep "FAIL\|RESULT" "$OUT/green.txt"
if [ $red -eq 4 ] && [ $rc -eq 0 ]; then echo "PASS: красный 4/4 пойман, зелёный $(grep -m1 RESULT "$OUT/green.txt" | cut -d' ' -f2)"; exit 0; fi
echo "FAIL: красный $red/4, зелёный rc=$rc"; exit 1
