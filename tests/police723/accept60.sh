#!/bin/bash
# №723 приёмка (Роб 17.10, видео IMG_7280 4:53: «полицейская машина въезжает на газон»; PS2: tests/police723/README в шапке msncar.c).
# Факт PS2 (build/ps2ref/answers b3_moving, b3_fine80, расчёт tests/police723/ps2_roads.py): к СТОЯЩЕМУ игроку полиция выезжает с дороги в ~45 м и подъезжает не ближе 27 м (b3_moving 27 м, b3_fine80 на возвышении 28,5 м),
# дальше держится 39–46 м и раз в ~11 с возвращается к месту появления; максимум от оси дороги 27,9 м. Порт до правки въезжал во двор до 13 м (31,7 м от оси) и вплотную (0,7 м) в переписи 22 мест.
# Проверки (хост, tools/bots, реальные дороги и COL1 TEST74): 1) сцена PS2 (игрок стоит у (122; 214) вне дорог): ближайшее за 40 с в 25…45 м, максимум от оси дороги ≤ 30 м; 2) перепись 22 мест (tests/hnr/police_host.c):
# ближе всего ≥ 25 м, RESULT PASS; 3) tests/hnr/chase430.c (полиция догоняет стоящего и едущего на L1–L7): 14 PASS. Красный: -DPOLICE_PS2STAND_OLD=1 — сцена 13 м / 31,7 м, перепись 0,7 м → FAIL.
# usage: tests/police723/accept60.sh [SET]  (SET — набор с level.shr, homer.shr, level01.msn, lN/level0N.msn; по умолчанию TEST74). Последняя строка PASS/FAIL
set -u; cd "$(dirname "$0")/../.."
SET=${1:-$HOME/Downloads/shar-psp-integ/build/test74/PSP/GAME/SHARTEST15}; OUT=${OUT:-build/p723}; mkdir -p $OUT; rc=0
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
V=viewer; . tools/bots/lib.sh
build() { cc -std=c11 -O2 -w -DSKIN_HOST $3 -I $V -I tools/bots -I tests/hnr $1 $BOTS_LIB -lm -o $2 || { echo "сборка $2 не удалась"; exit 2; }; }
for side in new old; do fl=""; [ $side = old ] && fl="-DPOLICE_PS2STAND_OLD=1"
  build tests/police723/track.c $OUT/gt_$side "$fl"; build tests/hnr/police_host.c $OUT/gp_$side "$fl"; build tests/hnr/chase430.c $OUT/gc_$side "$fl"; done
scene() { $OUT/gt_$1 $SET/level.shr $SET/homer.shr $SET/level01.msn 122 214 40 | tail -1; }
near() { $OUT/gt_$1 $SET/level.shr $SET/homer.shr $SET/level01.msn 122 214 40 | grep -E '^ *[0-9]+ +\(' | sed 's/.*до игрока \([0-9]*\) .*/\1/' | sort -n | head -1; }
for side in new old; do
  n=$(near $side); mo=$(scene $side | sed 's/.*максимум от оси \([0-9.]*\) м.*/\1/')
  echo "  сцена PS2 ($side): ближайшее за 40 с (по строкам раз в 25 кадров) $n м, максимум от оси $mo м"
  if [ $side = new ]; then awk -v n="$n" -v m="$mo" 'BEGIN{exit !(n>=25 && n<=45 && m<=30)}' || { echo "    !! вне 25…45 м / ≤ 30 м"; rc=1; }
  else awk -v n="$n" 'BEGIN{exit !(n<25)}' || { echo "    !! красный не красный"; rc=1; }; fi
done
for side in new old; do
  $OUT/gp_$side $SET/level.shr $SET/homer.shr $SET/level01.msn > $OUT/gp_$side.txt 2>&1; st=$(grep -E '^stand:' $OUT/gp_$side.txt | sed 's/.*ближе всего \([0-9.]*\) м.*/\1/'); res=$(grep -c 'RESULT PASS' $OUT/gp_$side.txt)
  echo "  перепись 22 мест ($side): ближе всего $st м, RESULT PASS=$res"
  if [ $side = new ]; then awk -v n="$st" 'BEGIN{exit !(n>=25)}' && [ "$res" = 1 ] || { echo "    !! ближе 25 м или провал переписи"; rc=1; }
  else awk -v n="$st" 'BEGIN{exit !(n<25)}' || { echo "    !! красный не красный"; rc=1; }; fi
done
$OUT/gc_new $SET/level01.msn $SET/l2/level02.msn $SET/l3/level03.msn $SET/l4/level04.msn $SET/l5/level05.msn $SET/l6/level06.msn $SET/l7/level07.msn > $OUT/gc_new.txt; r=$?
echo "  chase430 (догоняет стоящего и едущего, L1–L7): PASS $(grep -c PASS $OUT/gc_new.txt) FAIL $(grep -c FAIL $OUT/gc_new.txt) rc=$r"; [ $r = 0 ] || rc=1
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
