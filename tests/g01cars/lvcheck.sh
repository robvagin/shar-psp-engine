#!/bin/bash
# №1195 (g01-cars): проверка таблиц level.shr на настоящих файлах и подменах; красный контроль -DLVCHECK1195_OLD=1 (проверки нет) обязан падать. Каталог данных: LVDIR или набор TEST88 интегратора
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
D=${LVDIR:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}; mkdir -p build/g1195; rc=0
for k in new old; do F=""; [ $k = old ] && F="-DLVCHECK1195_OLD=1"
  cc -std=gnu11 -O1 -w -I viewer $F tests/g01cars/lvcheck.c -o build/g1195/lv_$k || exit 1
  echo "== $k"; build/g1195/lv_$k "$D" > build/g1195/lvout_$k.txt 2>&1; r=$?; tail -22 build/g1195/lvout_$k.txt
  if [ $k = new ]; then [ $r = 0 ] && echo "PASS: новое" || { echo "FAIL: новое (код $r)"; rc=1; }
  else [ $r != 0 ] && echo "красный контроль пойман (LVCHECK1195_OLD=1 падает)" || { echo "FAIL: красный не пойман"; rc=1; }; fi
done; exit $rc
