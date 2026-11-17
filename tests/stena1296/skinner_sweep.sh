#!/bin/bash
# 1309: Скиннер (L2 m1, этап 2 «оторваться») едет к стоящему игроку по дорогам из 13 точек уровня; хост (tests/stena1296/skinner_host.c, мир ботов, детерминированно): удары о стену дальше 30 м от игрока (wallV >= 12 м/с) и самая долгая остановка > 15 м от игрока.
#   skinner_sweep.sh [SET]    варианты: новое / SK_NOHEAD (только кривизна) / SK_OLD (прежнее)
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
S=${1:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}; V=viewer; . tools/bots/lib.sh; mkdir -p build/sk1309
cc -std=gnu11 -O2 -w -DSKIN_HOST -I $V -I tools/bots tests/stena1296/skinner_host.c $BOTS_LIB -lm -o build/sk1309/host || exit 2
T="92,-20 -150,-40 30,40 -150,-50 -150,0 -100,0 0,50 0,100 50,0 50,50 50,100 100,-50 100,0"
for var in new nohead old; do tw=0; tl=0; for t in $T; do x=${t%,*}; z=${t#*,}
  case $var in new) unset SK_OLD SK_NOHEAD;; nohead) unset SK_OLD; export SK_NOHEAD=1;; old) unset SK_NOHEAD; export SK_OLD=1;; esac
  r=$(build/sk1309/host $S/l2/level.shr $S/homer.shr $S/l2/level02.msn 40 $x $z 2>&1 | grep "stall_frames"); w=$(echo "$r" | sed 's/.*стен_далеко=\([0-9]*\).*/\1/'); l=$(echo "$r" | sed 's/.*longest=\([0-9.]*\).*/\1/'); a=$(echo "$r" | sed 's/.*прибыл=//')
  [ -n "${VERBOSE:-}" ] && echo "   $var $t: далеко=$w остановка=$l с прибыл=$a"; tw=$((tw + ${w:-0})); tl=$(python3 -c "print(max($tl, ${l:-0}))"); done
  echo "$var: ударов о стену далеко от игрока $tw, самая долгая остановка $tl с"; done
# вердикт: прежнее должно показывать дыру (>= 30 ударов), новое — не больше трети от прежнего и не меньше 9 из 13 доехали
