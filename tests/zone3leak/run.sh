#!/bin/bash
# 102: сторож утечки области города (ZONE3CITYLEAK_OLD / файл zone3cityleakold). tests/zone3leak/run.sh EBOOT   (набор test102tour с farcells, ballast 5; сценарии s27 Лестницы)
# g: tests/memladder/s27_dblload.txt - строк "WATCHDOG zone3 leak area=city" 0; r: s27_dblload_red.txt (flag dblguard 0) - >=1 строка, live=2; o: красный + zone3cityleakold - строк 0 (сторож выключен).
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; OUT=${OUT:-$W/build/zone3leak}; export OUT SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SHIP_LEVELS="2 3 4 5 6 7"; mkdir -p $OUT/ovl; export OVL=$OUT/ovl
export SET=${SET:-$HOME/Downloads/shar-psp-t93/build/test102tour/PSP/GAME/SHARTEST15}; cd $W; rc=0
for v in "g:s27_dblload.txt:" "r:s27_dblload_red.txt:" "o:s27_dblload_red.txt:zone3cityleakold"; do IFS=: read n sc fl <<< "$v"
  FLAGS="$fl" nice -n 20 tools/emu_run.sh tests/memladder/$sc $n $EB 900 > $OUT/$n.log 2>&1
  c=$(grep -c "WATCHDOG zone3 leak area=city" $OUT/out_$n/smoke.txt); echo "$n: строк leak area=city $c"
  case $n in g|o) [ "$c" = 0 ] || rc=1;; r) [ "$c" -ge 1 ] || rc=1;; esac
  find $OUT/out_$n -type f \( -name smoke.txt -o -name blackbox.txt -o -name headless.log -o -name '*.bmp' \) -delete; rm -rf $OUT/emu_home_$n $OUT/rr_$n
done; [ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
