#!/bin/bash
# сверка g01-cars №1159: сон/крупный шаг чужих машин против прежнего счёта (SV_CARCOST1159_OLD=1), в двух частотах кадра
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
O=build/g1159s; rm -rf $O; mkdir -p $O; rsync -a --exclude 'build*' viewer $O/; python3 tests/g01cars/cost_prep.py $O/viewer || exit 1
for k in new old; do F=""; [ $k = old ] && F="-DSV_CARCOST1159_OLD=1"; cc -std=c11 -O2 -w -I $O/viewer $F tests/g01cars/sleep.c $O/viewer/car.c $O/viewer/col.c -lm -o $O/sleep_$k || exit 1; done
for hz in 30 60; do for k in old new; do echo "== $k"; $O/sleep_$k famil_v $hz; done; done
