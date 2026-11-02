#!/bin/bash
# №1100: окно колец потоков 1024 Б вместо 2048 — звук бит в бит тот же (хеш вывода), байт в задании вдвое меньше, красные: окно 256 Б (хеш уходит/нехватка), MEHAND1100_OLD=1 (экономии нет)
set -u; cd "$(dirname "$0")/../.."; export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"; O=build/g08sound; mkdir -p $O; bad=0
b() { cc -std=c11 -O2 -w -DAUDIO_HOST $2 -I viewer tests/g08sound/hand.c -lm -o "$O/$1" || exit 2; }
b hand_g ""; b hand_r256 "-DSWIN_NEED=256"; b hand_old "-DMEHAND1100_OLD=1"
f() { sed -n 's/.*'"$2"'=\([^ ]*\).*/\1/p' <<<"$1"; }
for L in 1 5; do P=build/snd6/l$L/
  on=$($O/hand_g $P 1 6000); off=$($O/hand_g $P 0 6000); r=$($O/hand_r256 $P 1 6000); ol=$($O/hand_old $P 1 6000)
  hon=$(f "$on" HASH); hoff=$(f "$off" HASH); won=$(f "$on" WINB); woff=$(f "$off" WINB)
  [ "$hon" = "$hoff" ] && echo "L$L: хеш звука с окном 1024 = хеш с окном 2048 ($hon)" || { echo "FAIL L$L: звук разошёлся $hon / $hoff"; bad=1; }
  awk -v a="$won" -v b="$woff" 'BEGIN{exit !(a < 0.6*b)}' && echo "L$L: окон в задании за буфер $won Б против $woff Б" || { echo "FAIL L$L: экономии нет $won / $woff"; bad=1; }
  [ "$(f "$on" UNDERRUN)" = 0 ] && echo "L$L: нехватки окна нет" || { echo "FAIL L$L: нехватка окна"; bad=1; }
  hr=$(f "$r" HASH); ur=$(f "$r" UNDERRUN)
  if [ "$hr" != "$hon" ] || [ "$ur" != 0 ]; then echo "L$L: красный (окно 256 Б) пойман: хеш $hr, нехватка $ur"; else echo "FAIL L$L: красный окно 256 Б не пойман"; bad=1; fi
  [ "$(f "$ol" WINB)" = "$woff" ] && echo "L$L: красный MEHAND1100_OLD=1 даёт прежнее окно ($woff Б)" || { echo "FAIL L$L: MEHAND1100_OLD не вернул прежнее"; bad=1; }
done
[ $bad = 0 ] && echo "PASS: №1100 окно потоков" || { echo FAIL; exit 1; }
