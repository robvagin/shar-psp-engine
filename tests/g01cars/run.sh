#!/bin/bash
# Прогон проверок g01-cars: каждая строка таблицы — тест, ключ значения, границы нового, выключатель(и) старого.
# Новое значение обязано лежать в границах; с выключателем =1 — вне границ (тест падает на старом). Итог: строки OK / FAIL, код возврата 0 только если всё OK.
export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.." || exit 1
OUT=${OUT:-build/g01cars}; mkdir -p "$OUT"; rm -f "$OUT"/old_* "$OUT"/new
CC="cc -std=c99 -O2 -w -I viewer -DSV_FORCE_PLAYER=1 -DSV_AIRWHEEL_OLD=0"   # №1126: по умолчанию в игре призрачные колёса выключены; тест проверяет сам механизм, когда он включён
SRC="tests/g01cars/check.c tests/g01cars/surf.c viewer/col.c viewer/car.c -lm"
$CC -o "$OUT/new" $SRC || exit 1
# тест | ключ | нижняя | верхняя | выключатели старого (через +) | SAME — старое значение совпадает по построению (тест подтверждает число оригинала, но не падает)
# №1172: потеря сцепления ведущих на скорости SV_GRIPLOSS_K 0,05 вместо 0,2 по тексту исходника (с 0,2 TEST87 срывало в занос 38 машин из 89), граница griploss_hi 17,3…19,0 → 16,0…16,6
# №1126 (PS2-коридоры): границы steer_top, brake_limit, brake_force, griploss сдвинуты коэффициентами SV_STEER_DROP_K = 0,7 (до доли скорости 0,55, дальше к 1 у TopSpeed) и SV_ROLL_BRAKE_K = 0,2 (карточка tests/cars/attitude.c E и G)
TABLE='steer_top steer_top 0.28 0.36 SV_STEER_DROP_OLD
steer_low steer_low_deg 37 39.5 SV_STEER_LOW_OLD
steer_rate steer_rate 0.30 0.37 SV_STEER_RATE_OLD
hblatch hblatch 0.9 1.1 SV_HBLATCH_OLD
brake_limit brake_limit_decel 9.5 13 SV_BRAKE_LIMIT_OLD
brake_force brake_force 16.5 20.5 SV_BRAKE_DRIVEN_OLD
coast coast 3.4 3.8 SV_COAST_OLD
hb_throttle hb_throttle_speed 0.4 1.2 SV_HB_THROTTLE_OLD
burst_cap burst_cap_secs 0.45 0.55 SV_BURSTRATE_OLD
burst_cancel burst_cancel_secs 0 0.05 SV_BURSTRATE_OLD
reverse reverse_kmh 85 112 SV_REVERSE_OLD
topspeed top_accel 1.45 1.85 SV_COAST_OLD+SV_AIRDRAG_OLD SAME
jturn_drop jturn_latK 0 0.05 SV_JTURNDROP_OLD
jturn_slow jturn_slow_flag 0 0.5 SV_JTURNDROP_OLD
airstab_tilt airstab_tilt_ratio 0.8 1.0 SV_AIRSTAB_OLD
airdamp airdamp_ratio 0.9 1.1 SV_AIRSTAB2_OLD
weeble_diag weeble_roll_deg 18 24 SV_WEEBLEDIAG_OLD
surface_grip surface_grass_ratio 0.67 0.78 SV_SURFACE_GRIP_OLD
gripLoss griploss_hi 16.0 16.6 SV_GRIPLOSS_OLD
flipsteer flipsteer_ratio 0.97 1.03 SV_FLIPSTEER_OLD
rest_slope rest_slope_move 0 0.5 SV_REST_OLD
hitter hitter_ratio 0.45 0.55 SV_HITTERVEL_OLD
beef beef_ratio 0.95 1.05 SV_HPSTEAL2_OLD
boomplayer boom_player_damage 0.15 0.18 SV_BOOMPLAYER_OLD
huskabort huskabort_vel 99 101 SV_HUSKABORT_OLD
speedcap speedcap 0 1 SV_SPEEDCAP_OLD
noonehit noonehit_damage 0.99 0.9995 SV_NOONEHIT_OLD
damage damage_ratio 0.97 1.03 SV_DMG_IMPULSE_OLD
airwheel airwheel_speed 1.8 2.3 SV_AIRWHEEL_OLD
zone_boost zone_accel 12.3 14.5 SV_JUMPBOOST_OLD
zone_dir zone_against_boosts 0 0.5 SV_ZONEDIR_OLD'
fail=0; n=0
while read -r t key lo hi olds same; do
  [ -z "$t" ] && continue
  flags=""; for o in ${olds//+/ }; do flags="$flags -D$o=1"; done
  tag=$(echo "$olds" | tr '+' '_'); [ -x "$OUT/old_$tag" ] || $CC $flags -o "$OUT/old_$tag" $SRC || exit 1
  vn=$("$OUT/new" "$t" | awk -v k="$key" '$1==k{print $2}'); vo=$("$OUT/old_$tag" "$t" | awk -v k="$key" '$1==k{print $2}')
  okn=$(awk -v v="$vn" -v a="$lo" -v b="$hi" 'BEGIN{print (v>=a && v<=b)?1:0}'); oko=$(awk -v v="$vo" -v a="$lo" -v b="$hi" 'BEGIN{print (v>=a && v<=b)?1:0}')
  n=$((n+1)); if [ "$same" = SAME ] && [ "$okn" = 1 ]; then echo "SAME $t: $key новое $vn в [$lo,$hi]; $olds=1 даёт $vo — формула та же, на старом не падает (значения совпали с оригиналом и раньше)"
  elif [ "$okn" = 1 ] && [ "$oko" = 0 ]; then echo "OK   $t: $key новое $vn в [$lo,$hi]; $olds=1 даёт $vo (вне границ)"
  else echo "FAIL $t: $key новое $vn, границы [$lo,$hi], $olds=1 даёт $vo"; fail=$((fail+1)); fi
done <<< "$TABLE"
echo "проверок $n, провалов $fail"; exit $fail
