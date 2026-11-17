#!/bin/bash
# Гейт правки 1278 (подвеска как у оригинала: ход от нейтрали ±lim, пружина со знаком, отбой тянет вниз; сверка Fable-2 М1/М2, окно «Гомер» 107b).
# Числа PS2: пределы смещения колеса ±0,266 м, в покое +0,133 (записи s_ps2_w_jz16_70/jz17_70); 14 прыжков twin против PS2 (pitch_ps2.tsv, ramps_ps2.tsv); езда на двух колёсах (tests/cars/cornering.c).
# 13.11 ~22:50 ОБНОВЛЕНО: признак зоны прыжка по входу в объём (Fable-2) сделал кандидата равным прежней подвеске на 10 записях PS2 (склон 2,162 / кромка 41,1° и 11,25 против 2,159 / 40,6° и 11,20), гейт ниже подгонялся под 4 записи и больше не зелёный: информационный, не приёмка. Подвеска ВЫКЛ.
# Зелёный (новое): twin 0 вне допуска; Σ|Δvy| за 3 м до кромки ≤ 9,5 м/с (PS2-эталон; прежнее 10,51); СКО хода по склону ≤ 2,12 (прежнее 2,25); заездов на двух колёсах >10% не больше 40 (прежнее 45).
# Подвеска по умолчанию ВЫКЛЮЧЕНА (кандидат), гейт собирает её явно -DCAR_SUSP1278_OLD=0. Красный (-DCAR_SUSP1278_OLD=1): те же три числа остаются прежними и проверка «не лучше» падает.   tests/car107b/susp1278.sh [level.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/c107b/gate1278; rm -rf $OUT; mkdir -p $OUT; V=viewer
LV=${1:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15/level.shr}; [ -f "$LV" ] || { echo "FAIL: нет $LV"; exit 1; }
HOST="$V/car.c $V/col.c $V/props.c $V/stateprops.c $V/coins.c"
for k in new old; do
  F=(-DCAR_SUSP1278_OLD=0); [ $k = old ] && F=(-DCAR_SUSP1278_OLD=1); mkdir -p $OUT/$k
  cc -std=c11 -O2 -w -DSKIN_HOST -I $V "${F[@]}" tests/jumps/twin.c $HOST -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/twin || { echo "FAIL: сборка twin"; exit 1; }
  $OUT/$k/twin "$LV" --trace $OUT/$k > $OUT/$k/twin.txt 2>&1
  cc -std=c11 -O2 -w -I $V "${F[@]}" tests/cars/cornering.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/corner 2>/dev/null && $OUT/$k/corner --max-flips 10 > $OUT/$k/corner.txt 2>&1
done
m() { python3 tests/jumps/pitchcmp.py $OUT/$1 | LC_ALL=C awk -v w="$2" '{ if (w=="vy3") { for(i=1;i<=NF;i++) if($i=="до") {} } } END{}' ; }
vy3() { python3 tests/jumps/pitchcmp.py $OUT/$1 | head -1 | sed -E 's/.*за 3 м до кромки: Σ\|Δтангаж\| [0-9.]+° Σ\|Δvy\| ([0-9.]+).*/\1/'; }
rms() { python3 tests/jumps/pitchcmp.py $OUT/$1 | sed -n 2p | sed -E 's/.*СКО ([0-9.]+).*/\1/'; }
tw() { LC_ALL=C grep -a -c ' FAIL' $OUT/$1/twin.txt; }
two() { LC_ALL=C grep -a 'RESULT' $OUT/$1/corner.txt | tail -1 | sed -E 's/.*two-wheel>10% ([0-9]+).*/\1/'; }
echo "новое: twin вне допуска $(tw new), Σ|Δvy| за 3 м $(vy3 new), СКО склона $(rms new), на двух колёсах $(two new) | старое: twin $(tw old), $(vy3 old), $(rms old), на двух колёсах $(two old)"
ok() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<=b)}'; }
fail=0
{ [ "$(tw new)" = 0 ] && ok "$(vy3 new)" 9.5 && ok "$(rms new)" 2.12 && ok "$(two new)" 40; } || { echo "FAIL зелёный"; fail=1; }
{ ! ok "$(vy3 old)" 9.5 && ! ok "$(rms old)" 2.12 && ! ok "$(two old)" 40; } && echo "красный CAR_SUSP1278_OLD=1: упал, как должен (три числа прежние)" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
