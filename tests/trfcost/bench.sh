#!/bin/bash
# №1159: цена кадра трафика (tests/trfcost/bench.c). Печатает среднее и худшее время trafficUpdate; с двумя деревьями — сравнение: BASE=путь к дереву TEST83 (viewer/ внутри)
set -e
cd "$(dirname "$0")/../.."; S=${S:-$HOME/Downloads/shar-psp/build}; L=${1:-$S/level.shr}; T=${2:-$S/traffic.shr}; H=${3:-$S/homer.shr}
O=${TMPDIR:-/tmp}/trfcost.$$; mkdir -p "$O"
for tree in ${BASE:+$BASE} .; do
  cc -std=c11 -O2 -w -DSKIN_HOST $EXTRA -I $tree/viewer tests/trfcost/bench.c $tree/viewer/traffic.c $tree/viewer/homer.c $tree/viewer/skin.c $tree/viewer/col.c $tree/viewer/car.c -lm -o "$O/b"
  echo "[$tree $EXTRA] $("$O/b" "$L" "$T" "$H")"
done
rm -f "$O/b"; rmdir "$O"
