#!/bin/bash
# Гейт подвески 108 (правки 1278/центр масс/пружины, Роб 14.11 «подвеску тоже в 108»): модель оригинала — ход вниз до −0,266 м от нейтрали (0,39 м от упора до упора вниз), пружина со знаком, одна жёсткость 27 047 на все колёса
# (CAR_SUSP1278_OLD=0, CAR_SPR109_OLD=0). Центр масс остаётся по данным диска (CAR_COG109_OLD=1): запись покоя PS2 даёт передние колёса сжаты больше задних (0,1445 / 0,1212) и тангаж −0,40° — это следствие
# одной жёсткости при центре масс ВПЕРЕДИ середины базы; «центр масс посередине базы» (COG109_OLD=0) даёт тангаж 0 и ломает 2–3 посадки twin.
# Проверки против записей PS2 (все, что есть): покой famil_v; уступ вверх 30 км/ч (a3_curb_up_30); склон и кромка по 10 заездам (twin, slopecmp: СКО склона, Σ|Δтангаж| и Σ|Δvy| на кромке); 14 прыжков twin (0 вне допуска); заезды на двух колёсах (cornering).
# Красный: -DCAR_SUSP1278_OLD=1 -DCAR_SPR109_OLD=1 (прежняя подвеска) — каждая проверка роста точности должна упасть.   tests/car109/susp108.sh [level.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; V=viewer; OUT=build/c109/g108; rm -rf $OUT; mkdir -p $OUT
LV=${1:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15/level.shr}; [ -f "$LV" ] || { echo "FAIL: нет $LV"; exit 1; }
HOST="$V/car.c $V/col.c $V/props.c $V/stateprops.c $V/coins.c"
for k in new old; do
  F=(); [ $k = old ] && F=(-DCAR_SUSP1278_OLD=1 -DCAR_SPR109_OLD=1); mkdir -p $OUT/$k
  cc -std=c11 -O2 -w -DSKIN_HOST -I $V ${F[@]+"${F[@]}"} tests/jumps/twin.c $HOST -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/twin || { echo "FAIL: сборка twin"; exit 1; }
  $OUT/$k/twin "$LV" --trace $OUT/$k > $OUT/$k/twin.txt 2>&1
  cc -std=c11 -O2 -w -I $V ${F[@]+"${F[@]}"} tests/car109/rest.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/rest && $OUT/$k/rest > $OUT/$k/rest.txt
  cc -std=c11 -O2 -w -I $V ${F[@]+"${F[@]}"} tests/car109/curb.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/curb && $OUT/$k/curb up > $OUT/$k/curb.txt
  cc -std=c11 -O2 -w -I $V ${F[@]+"${F[@]}"} tests/cars/cornering.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/$k/corner 2>/dev/null && $OUT/$k/corner --max-flips 10 > $OUT/$k/corner.txt 2>&1
done
sl=$(python3 -c "print(1)") # заглушка
pit() { sed -E 's/.*pitch ([-0-9.]+)$/\1/' $OUT/$1/rest.txt; }
cb() { awk -v a="$2" '{for(i=1;i<=NF;i++) if ($i=="тангаж") {split($(i+1),p,"\\.\\."); } } END{}' $OUT/$1/curb.txt; python3 - "$OUT/$1/curb.txt" "$2" <<'P'
import re,sys
t=open(sys.argv[1]).read(); k=sys.argv[2]
m=re.search(r'тангаж ([-\d.]+)\.\.([-\d.]+) vy ([-\d.]+)\.\.([-\d.]+) y-подъём ([-\d.]+) xs_min_контакт ([-\d.]+) xs_max ([-\d.]+) колёс-кадров с xs<0 (\d+)',t)
pmin,pmax,vmin,vmax,rise,xmin,xmax,neg=m.groups()
if k=='err': print('%.3f'%(abs(float(pmax)-5.36)+abs(float(pmin)+3.32)+abs(float(vmax)-1.16)+abs(float(rise)-0.29)))
else: print(neg)
P
}
tw() { LC_ALL=C grep -a -c ' FAIL' $OUT/$1/twin.txt; }
vy3() { python3 tests/jumps/pitchcmp.py $OUT/$1 | head -1 | sed -E 's/.*кромка: Σ\|Δтангаж\| ([0-9.]+)° Σ\|Δvy\| ([0-9.]+).*/\2/'; }
lip() { python3 tests/car107b/slopecmp.py $OUT/$1 | tail -1 | sed -E 's/.*Σ\|Δтангаж\| ([0-9.]+)° Σ\|Δvy\| ([0-9.]+).*/\1/'; }
rms() { python3 tests/car107b/slopecmp.py $OUT/$1 | sed -E -n 's/.*склон_СКО=([0-9.]+).*/\1/p' | head -1; }
two() { LC_ALL=C grep -a 'RESULT' $OUT/$1/corner.txt | tail -1 | sed -E 's/.*two-wheel>10% ([0-9]+).*/\1/'; }
echo "новое: покой тангаж $(pit new)° (PS2 −0,40), уступ 30 вверх: ошибка по 4 числам $(cb new err) (0 = запись), колёс-кадров ниже нейтрали $(cb new neg) (PS2 23), склон СКО $(rms new), кромка тангаж Σ $(lip new)°, twin вне допуска $(tw new), на двух колёсах $(two new)"
echo "старое: покой тангаж $(pit old)°, уступ ошибка $(cb old err), ниже нейтрали $(cb old neg), склон СКО $(rms old), кромка тангаж Σ $(lip old)°, twin $(tw old), на двух колёсах $(two old)"
le() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<=b)}'; }; ge() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }
fail=0
{ le "$(pit new)" -0.25 && le "$(cb new err)" 1.2 && ge "$(cb new neg)" 1 && le "$(rms new)" 2.15 && le "$(lip new)" 39 && [ "$(tw new)" = 0 ] && le "$(two new)" 40; } || { echo "FAIL зелёный"; fail=1; }
{ ! le "$(pit old)" -0.25 && ! le "$(cb old err)" 1.2 && ! ge "$(cb old neg)" 1 && ! le "$(lip old)" 39 && ! le "$(two old)" 40; } && echo "красный (прежняя подвеска): упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
