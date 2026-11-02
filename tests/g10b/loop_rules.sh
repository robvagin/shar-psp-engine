#!/bin/bash
# Круг 6 g10b-loop: tests/g10b/loop_rules.c. ЗЕЛЁНЫЙ — всё PASS. КРАСНЫЕ: по одному выключателю -D<ИМЯ>_OLD=1 — падают проверки именно этого имени и никакие другие.
# Хост, Мак, CPU, без эмулятора. SDKROOT нужен, пока не принята лицензия Xcode.
set -u
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/g10b_rules}; mkdir -p "$OUT"
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
build() { cc -std=c11 -O0 -w -I viewer "$@" tests/g10b/loop_rules.c -lm -o "$OUT/t_$1" 2>"$OUT/cc.txt" || { echo "FAIL: сборка"; head -5 "$OUT/cc.txt"; exit 2; }; }
cc -std=c11 -O0 -w -I viewer tests/g10b/loop_rules.c -lm -o "$OUT/t_new" 2>"$OUT/cc.txt" || { echo "FAIL: сборка"; head -5 "$OUT/cc.txt"; exit 2; }
"$OUT/t_new" > "$OUT/new.log"; rc=$?
echo "зелёный: rc=$rc, проверок $(grep -c '^CHECK' "$OUT/new.log"), упало $(grep -c 'FAIL$' "$OUT/new.log")"
fail=0; [ $rc -ne 0 ] && fail=1
# №1130: вид назад убран; прежний режим (TEST84) проверяется вторым зелёным с -DLOOKBACK1130_OLD=1 и красным REARVIEW_OLD=1 поверх него
cc -std=c11 -O0 -w -I viewer -DLOOKBACK1130_OLD=1 tests/g10b/loop_rules.c -lm -o "$OUT/t_rear" 2>"$OUT/cc.txt" || { echo "FAIL: сборка rear"; exit 2; }
"$OUT/t_rear" > "$OUT/rear.log"; rrc=$?; oth=$(grep "^CHECK .* FAIL\$" "$OUT/rear.log" | grep -vc "^CHECK LOOKBACK1130_OLD "); echo "прежний режим (LOOKBACK1130_OLD=1): проверок $(grep -c '^CHECK' "$OUT/rear.log")"; [ "$oth" -ne 0 ] && fail=1
cc -std=c11 -O0 -w -I viewer -DLOOKBACK1130_OLD=1 -DREARVIEW_OLD=1 tests/g10b/loop_rules.c -lm -o "$OUT/t_rear_red" 2>"$OUT/cc.txt" || exit 2
"$OUT/t_rear_red" > "$OUT/rear_red.log"; rm_=$(grep -c "^CHECK REARVIEW_OLD .* FAIL\$" "$OUT/rear_red.log"); ro_=$(grep "^CHECK .* FAIL\$" "$OUT/rear_red.log" | grep -v "^CHECK REARVIEW_OLD " | grep -vc "^CHECK LOOKBACK1130_OLD ")
if [ "$rm_" -gt 0 ] && [ "$ro_" -eq 0 ]; then echo "красный REARVIEW_OLD=1 (поверх прежнего режима): упало $rm_ своих, чужих 0 — ок"; else echo "КРАСНЫЙ REARVIEW НЕ ТОТ: своих $rm_, чужих $ro_"; fail=1; fi
for m in $(grep -o '^CHECK [A-Z0-9_]*_OLD' "$OUT/new.log" | awk '{print $2}' | sort -u); do
  cc -std=c11 -O0 -w -I viewer -D$m=1 tests/g10b/loop_rules.c -lm -o "$OUT/t_$m" 2>"$OUT/cc.txt" || { echo "FAIL: сборка $m"; exit 2; }
  "$OUT/t_$m" > "$OUT/$m.log"
  mine=$(grep -c "^CHECK $m .* FAIL\$" "$OUT/$m.log"); tot=$(grep -c "^CHECK $m " "$OUT/$m.log"); other=$(grep "^CHECK .* FAIL\$" "$OUT/$m.log" | grep -vc "^CHECK $m ")
  if [ "$mine" -gt 0 ] && [ "$other" -eq 0 ]; then echo "красный $m=1: упало $mine из $tot своих, чужих 0 — ок"; else echo "КРАСНЫЙ НЕ ТОТ $m: своих упало $mine из $tot, чужих $other"; fail=1; fi
done
# вызовы есть в игре (урок круга 3: «внедрено», а вызова нет): каждое правило использует игровой код
for f in g10bHnrRate:main_loop.h g10bPauseBanned:main_loop.h g10bRearCam:main_loop.h g10bBumperFovY:main_loop.h g10bSmokeStage:main_loop.h g10bSmokeRate:main_loop.h g10bTurboStep:main_loop.h g10bLandSpark:main_loop.h g10bCarZoneCorner:main_loop.h g10bViewNext:main_loop.h g10bLevelAsk:main_loop.h g10bTriStep:main_loop.h g10bTriFired:main_loop.h g10bMenuResumeShown:main_levels.h g10bMenuCursorResume:main_levels.h g10bMenuScrapEnabled:main_levels.h g10bLicenseMs:main_levels.h g10bFmvNoSkip:main_levels.h g10bLoadMinS:main_frontend.h g10bTutPauseOpens:main_loop.h g10bLbExitSecs:main_loop.h g10bLbConvStart:main_loop.h g10bLbCover:main_loop.h g10bLbConvDone:main_loop.h g10bTalkBlendSecs:main_loop.h g10bCamUnstable:main_loop.h g10bRearLook:main_loop.h g10bBottomSparkStep:main_loop.h g10bBottomOut:main_loop.h g10bWorldCarAway:game_msn.c g10bEvery:game_msn.c; do
  fn=${f%%:*}; fl=${f##*:}; if grep -q "$fn" "viewer/$fl"; then :; else echo "НЕТ ВЫЗОВА: $fn в viewer/$fl"; fail=1; fi; done
[ $fail -eq 0 ] && echo "PASS g10b loop_rules" || echo "FAIL g10b loop_rules"; exit $fail
