#!/bin/bash
# Гейт сверки g11b на Маке: осы (W1–W4). Зелёный — настоящий код; красные контроли — прежнее поведение обязано упасть.
#   tests/collect/sverka_g11b.sh [PACK]   по умолчанию build/collect-l1.shr
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PACK="${1:-build/collect-l1.shr}"; OUT=build/collect; mkdir -p "$OUT"; V=viewer
. tools/bots/lib.sh
b() { cc -std=c11 -O2 -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots "$@" tests/collect/sverka_g11b.c $BOTS_LIB -lm -o "$OUT/sverka_$N" 2> "$OUT/sverka_$N.build.txt" || { echo "FAIL: сборка $N"; head -5 "$OUT/sverka_$N.build.txt"; exit 1; }; }
N=green; b
N=red_aggr; b -DWASP_AGGR_OLD=1
N=red_kmh; b -DWASP_BOLT_KMH_OLD=1
"$OUT/sverka_green" "$PACK" | tee "$OUT/sverka_green.txt"; g=${PIPESTATUS[0]}
"$OUT/sverka_red_aggr" "$PACK" > "$OUT/sverka_red_aggr.txt"; ra=$?
"$OUT/sverka_red_kmh" "$PACK" > "$OUT/sverka_red_kmh.txt"; rk=$?
echo "зелёный rc=$g, красный WASP_AGGR_OLD rc=$ra, красный WASP_BOLT_KMH_OLD rc=$rk"
[ "$g" = 0 ] && [ "$ra" = 1 ] && [ "$rk" = 1 ] && echo "PASS: зелёный проходит, оба красных краснеют" || { echo "FAIL"; exit 1; }
# круг 5 (P11-Д5): рамка забора в луче осы
cc -std=c11 -O1 -DSKIN_HOST -I $V tests/collect/fencebox_g11b.c $V/col.c -lm -o "$OUT/fencebox" 2> "$OUT/fencebox.build.txt" && "$OUT/fencebox" || { echo "FAIL fencebox"; exit 1; }
# круг 6 (P03-01/02/03/04/27/29): монеты и потеря монет при взрыве; рисунок монет — наши спрайты, тест только логика и числа
cc6() { cc -std=c11 -O1 -Wno-unused-function -DSKIN_HOST -I $V "$@" tests/collect/coins_g11b.c $V/coins.c $V/col.c -lm -o "$OUT/coins_$N" 2> "$OUT/coins_$N.build.txt" || { echo "FAIL: сборка coins_$N"; exit 1; }; }
N=green; cc6; "$OUT/coins_green" | tee "$OUT/coins_green.txt" | grep -c "^PASS" | sed 's/^/монеты: PASS строк /'; "$OUT/coins_green" > /dev/null || { echo "FAIL coins green"; exit 1; }
for sw in COIN_AGE_OLD COIN_EXPIRE_OLD COIN_LAUNCH_OLD COIN_BOUNCE_OLD CAR_BOOM_COINS20_OLD CAR_BOOM_ENEMY_COINS_OLD; do N=$sw; cc6 -D$sw=1
  "$OUT/coins_$sw" > "$OUT/coins_$sw.txt"; [ $? -eq 1 ] && echo "красный $sw: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $sw"; exit 1; }; done
# круг 6 (осы W2/W4/W5/W8/W9/W11/W12/Д12, 8.3): tests/collect/wasps_g11b.c; каждая строка падает на своём _OLD=1
cw6() { . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/collect/wasps_g11b.c $BOTS_LIB -lm -o "$OUT/wasps_$N" 2> "$OUT/wasps_$N.build.txt" || { echo "FAIL: сборка wasps_$N"; exit 1; }; }
N=green; cw6; "$OUT/wasps_green" "$PACK" > "$OUT/wasps_green.txt" || { echo "FAIL wasps green"; grep ^FAIL "$OUT/wasps_green.txt"; exit 1; }; echo "осы круг 6: PASS строк $(grep -c '^PASS' "$OUT/wasps_green.txt")"
for sw in WASP_AGGR_OLD WASP_EVADE_OLD WASP_AIM_OLD WASP_BOLT_HIT_OLD WASP_BOB_OLD WASP_NEAR_OLD WASP_CARD_AGGR_OLD WASP_SPAWN_ORIG_OLD WASP_GAGPAUSE_OLD WASP_RANGEFLY_OLD; do N=$sw; cw6 -D$sw=1
  "$OUT/wasps_$sw" "$PACK" > "$OUT/wasps_$sw.txt"; [ $? -eq 1 ] && echo "красный $sw: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $sw"; exit 1; }; done
# круг 6 (гэги P16-14/Д5/Д7, карточка P05:9): tests/collect/gags_g11b.c
cg6() { bash -c ". tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots $* tests/collect/gags_g11b.c \$BOTS_LIB -lm -o $OUT/gags_$N" 2> "$OUT/gags_$N.build.txt" || { echo "FAIL: сборка gags_$N"; exit 1; }; }
export V
N=green; cg6; "$OUT/gags_green" "$PACK" > "$OUT/gags_green.txt" || { echo "FAIL gags green"; grep ^FAIL "$OUT/gags_green.txt"; exit 1; }; echo "гэги и карточка круг 6: PASS строк $(grep -c '^PASS' "$OUT/gags_green.txt")"
for sw in GAG_SOUNDWAIT_OLD GAG_COINS_AUTO_OLD GAG_MISSION_OLD CARD_EXACT_OLD GAG_OUTRO_OLD GAG_INTRO_OLD CARD_CAR15_OLD GAG_CAR15_OLD; do N=$sw; cg6 -D$sw=1
  "$OUT/gags_$sw" "$PACK" > "$OUT/gags_$sw.txt"; [ $? -eq 1 ] && echo "красный $sw: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $sw"; exit 1; }; done
python3 tests/collect/evade_table.py > build/collect/evade.txt && python3 tests/collect/evade_table.py --red > /dev/null; [ $? -eq 1 ] && echo "красный evade_table: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: evade_table"; exit 1; }
# №1105: данные альбома SCRAP BOOK из сохранения (tests/collect/scrap1105.c); красные контроли: прежний альбом, сдвиг бита награды, процент без 0,99
cs5() { bash -c ". tools/bots/lib.sh; cc -std=c11 -O1 -w -DSKIN_HOST -I $V -I tools/bots $* tests/collect/scrap1105.c $V/savegame.c \$BOTS_LIB -lm -o $OUT/scrap1105_$N" 2> "$OUT/scrap1105_$N.build.txt" || { echo "FAIL: сборка scrap1105_$N"; exit 1; }; }
N=green; cs5; "$OUT/scrap1105_green" > "$OUT/scrap1105_green.txt" || { echo "FAIL scrap1105 green"; grep ^FAIL "$OUT/scrap1105_green.txt"; exit 1; }; echo "альбом №1105: PASS строк $(grep -c '^PASS' "$OUT/scrap1105_green.txt")"
for sw in SCRAPBOOK1105_OLD=1 SCRAP_RED_OWNEDBIT SCRAP_RED_PCT; do N=${sw%%=*}; cs5 -D$sw; "$OUT/scrap1105_$N" > "$OUT/scrap1105_$N.txt"; [ $? -eq 1 ] && echo "красный $sw: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $sw"; exit 1; }; done
# №1118: блик на зеркале Moe's и BartRoom (viewer/mirrorglint.h, tests/rooms/glint1118.c); красные контроли: блик против камеры, без зажима у крайней камеры
cc -std=c11 -O1 -w -o "$OUT/glint1118_green" tests/rooms/glint1118.c -lm && "$OUT/glint1118_green" > "$OUT/glint1118_green.txt" || { echo "FAIL glint1118 green"; grep ^FAIL "$OUT/glint1118_green.txt"; exit 1; }; echo "блик зеркал №1118: PASS строк $(grep -c '^PASS' "$OUT/glint1118_green.txt")"
for sw in GLINT_RED_SIGN GLINT_RED_NOCLAMP; do cc -std=c11 -O1 -w -D$sw -o "$OUT/glint1118_$sw" tests/rooms/glint1118.c -lm; "$OUT/glint1118_$sw" > "$OUT/glint1118_$sw.txt"; [ $? -eq 1 ] && echo "красный $sw: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: $sw"; exit 1; }; done
# круг 7 (P04 Д10): снаряд у стены встаёт на 0,75 м за поверхностью; мир собран вручную; красный контроль WASP_BOLT_BEHIND75_OLD=1
for v in green red; do D=""; [ $v = red ] && D="-DWASP_BOLT_BEHIND75_OLD=1"; bash -c ". tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots $D tests/collect/boltwall_g11b.c \$BOTS_LIB -lm -o $OUT/boltwall_$v" || { echo "FAIL: сборка boltwall_$v"; exit 1; }; done
"$OUT/boltwall_green" "$PACK" > "$OUT/boltwall_green.txt" || { echo "FAIL boltwall green"; cat "$OUT/boltwall_green.txt"; exit 1; }; echo "стена у снаряда круг 7: PASS"
"$OUT/boltwall_red" "$PACK" > "$OUT/boltwall_red.txt"; [ $? -eq 1 ] && echo "красный WASP_BOLT_BEHIND75_OLD: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: WASP_BOLT_BEHIND75_OLD"; exit 1; }
# круг 7 (P04 W5): снаряд осы и мир — крюк svBoltWorld; зелёный с крюком (взрыв у предмета, удар применён один раз) и без крюка (долетает до стены); красный WASPBOLTWORLD_OLD=1
for v in green red; do D=""; [ $v = red ] && D="-DWASPBOLTWORLD_OLD=1"; bash -c ". tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots $D tests/collect/boltworld_g11b.c \$BOTS_LIB -lm -o $OUT/boltworld_$v" || { echo "FAIL: сборка boltworld_$v"; exit 1; }; done
"$OUT/boltworld_green" "$PACK" > "$OUT/boltworld_green.txt" && "$OUT/boltworld_green" "$PACK" null > "$OUT/boltworld_null.txt" || { echo "FAIL boltworld green"; cat "$OUT/boltworld_green.txt" "$OUT/boltworld_null.txt"; exit 1; }; echo "снаряд и мир круг 7: PASS (с крюком и без)"
"$OUT/boltworld_red" "$PACK" > "$OUT/boltworld_red.txt"; [ $? -eq 1 ] && echo "красный WASPBOLTWORLD_OLD: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: WASPBOLTWORLD_OLD"; exit 1; }
# круг 7 (P04 W5): trafficBoltHit — пешеходы и трафик; красный TRF_BOLT_OLD=1
for v in green red; do D=""; [ $v = red ] && D="-DTRF_BOLT_OLD=1"; bash -c ". tools/bots/lib.sh; cc -std=c11 -O1 -w -DSKIN_HOST -I $V -I tools/bots $D tests/collect/boltraffic_g11b.c \$BOTS_LIB -lm -o $OUT/boltraffic_$v" || { echo "FAIL: сборка boltraffic_$v"; exit 1; }; done
"$OUT/boltraffic_green" > "$OUT/boltraffic_green.txt" || { echo "FAIL boltraffic green"; cat "$OUT/boltraffic_green.txt"; exit 1; }; echo "снаряд, пешеходы и трафик круг 7: PASS строк $(grep -c '^PASS' "$OUT/boltraffic_green.txt")"
"$OUT/boltraffic_red" > "$OUT/boltraffic_red.txt"; [ $? -eq 1 ] && echo "красный TRF_BOLT_OLD: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН: TRF_BOLT_OLD"; exit 1; }
