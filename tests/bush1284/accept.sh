#!/bin/bash
# 1284 (Роб: «всё как в оригинале»): круглые кусты (стейтпроп cls 2) твёрдые, как стена, на любой скорости (сверка P11-08). Хост на Маке, стенд tests/bush1101/host.c (одинокие кусты уровня, 4 направления, 3/5/10/14 м/с).
# Зелёный (по умолчанию): насквозь проходят не больше 20 % заездов (часть заездов уходит в обход углом или в воздухе) на каждой скорости. Красный (-DSPBUSH_SV_OLD=1 -DBUSHMID1101_OLD=1, прежнее «насквозь»): на 10 и 14 м/с проходят не меньше 45 %.
#   tests/bush1284/accept.sh [LEVEL.shr ...]
set -u; cd "$(dirname "$0")/../.."
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; LV=("$@"); [ ${#LV[@]} -eq 0 ] && { LV=($SET/level.shr); for n in 2 3 4 5 6 7; do LV+=($SET/l$n/level.shr); done; }
OUT=build/bush1284; mkdir -p $OUT; V=viewer
SRC="tests/bush1101/host.c $V/props.c $V/stateprops.c $V/col.c $V/car.c $V/coins.c"
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V "${@:2}" $SRC -lm -o $OUT/$1 || { echo "FAIL: сборка $1"; exit 1; }; : > $OUT/$1.txt; for l in "${LV[@]}"; do "$OUT/$1" "$l" >> $OUT/$1.txt 2>/dev/null; done; }
b green; b through -DSPBUSH_SV_OLD=1 -DBUSHMID1101_OLD=1
agg() { awk -v v="$2" -v key="$3" '$1==v { for (i=2;i<=NF;i+=2) if ($i==key) s+=$(i+1) } END { print s+0 }' $OUT/$1.txt; }
fail=0
for v in v3 v5 v10 v14; do gn=$(agg green $v n); gc=$(agg green $v crossed); tn=$(agg through $v n); tc=$(agg through $v crossed)
  echo "$v: зелёный проходят $gc из $gn; красный (насквозь) $tc из $tn"
  awk -v c=$gc -v n=$gn 'BEGIN{exit !(n>0 && c<=.2*n)}' || { echo "  FAIL зелёный: куст не держит"; fail=1; }
  case $v in v10|v14) awk -v c=$tc -v n=$tn 'BEGIN{exit !(n>0 && c>=.45*n)}' || { echo "  КРАСНЫЙ НЕ ПОЙМАН на $v"; fail=1; };; esac
done
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
