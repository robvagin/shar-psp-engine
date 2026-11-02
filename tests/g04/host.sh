#!/bin/bash
# Быстрая проверка окна g04-homer на Маке: новое (без флагов) и старое (все HOMER_*_OLD=1) поведение. Без эмулятора.
#   tests/g04/host.sh [level.shr] [homer.shr]
cd "$(dirname "$0")/../.."
L=${1:-$HOME/Downloads/shar-psp/build/level.shr}; Hm=${2:-$HOME/Downloads/shar-psp/build/homer.shr}
OLD=$(grep -o 'HOMER_[A-Z0-9_]*_OLD\|SKIN_[A-Z_]*_OLD' viewer/homer.h viewer/skin.h | sed 's/^[^:]*://' | sort -u | sed 's/^/-D/;s/$/=1/' | tr '\n' ' ')
T=${TMPDIR:-/tmp}/g04.$$; SRC="tests/g04/host_g04.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c"
cc -std=c99 -O2 -Wno-unused-function -DSKIN_HOST -I viewer $SRC -lm -Wl,-U,_uiFlash -o $T.new || exit 1
cc -std=c99 -O2 -Wno-unused-function -DSKIN_HOST $OLD -I viewer $SRC -lm -Wl,-U,_uiFlash -o $T.old || exit 1
r=0; $T.new "$L" "$Hm" new || r=1; $T.old "$L" "$Hm" old || r=1; rm -f $T.new $T.old; exit $r
