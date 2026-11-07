#!/bin/bash
# №1195 (g01-cars): замки с пределом и зонд стека. Красные контроли: -DLOCKLIM1195_OLD=1 (замок без предела — зависание) и -DRELSTACK1195_OLD=1 (стек не закрашивается) обязаны падать
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
mkdir -p build/g1195; rc=0
for k in new locklim stack; do F=""; [ $k = locklim ] && F="-DLOCKLIM1195_OLD=1"; [ $k = stack ] && F="-DRELSTACK1195_OLD=1"
  cc -std=gnu11 -O1 -w -I viewer $F tests/g01cars/reliab.c viewer/reliab.c -lpthread -o build/g1195/t_$k || exit 1
  echo "== $k"; build/g1195/t_$k > build/g1195/out_$k.txt 2>&1; r=$?; cat build/g1195/out_$k.txt | tail -8
  if [ $k = new ]; then [ $r = 0 ] && echo "PASS: новое" || { echo "FAIL: новое"; rc=1; }
  else [ $r != 0 ] && echo "красный контроль ($k) пойман" || { echo "FAIL: красный $k не пойман"; rc=1; }; fi
done; exit $rc
