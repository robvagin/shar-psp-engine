#!/bin/bash
# 108 П1: сравнение «страницы / плотная карта» на уровнях L1..L7: tests/opcar108/run_ab.sh EBOOT [уровни]  (SET с lmap.shr версии 2, OUT)
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; LV=${2:-1 2 3 4 5 6 7}
for L in $LV; do
  ( FLAGS="${NF:-lmapverify}" nice -n 20 tools/emu_run.sh $OUT/sc$L.txt n$L "$EB" 200 > $OUT/n$L.log 2>&1 ) &
  ( FLAGS="${OF:-p1pagesold}" nice -n 20 tools/emu_run.sh $OUT/sc$L.txt o$L "$EB" 200 > $OUT/o$L.log 2>&1 ) & wait
done
