#!/bin/bash
# С-г1 (109): новое (без флагов) проходит свои случаи; красный контроль — сборка -DHOMERHIT109_OLD=1 обязана вести себя как прежнее (сбивает боком и с крыши) и потому падать на ожиданиях «нового».
#   tests/stena109/homerhit_gate.sh [level.shr] [homer.shr]
cd "$(dirname "$0")/../.."
L=${1:-$HOME/Downloads/shar-psp/build/level.shr}; Hm=${2:-$HOME/Downloads/shar-psp/build/homer.shr}
T=${TMPDIR:-/tmp}/hh109.$$; SRC="tests/stena109/homerhit_host.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c"
cc -std=c99 -O2 -Wno-unused-function -DSKIN_HOST -I viewer $SRC -lm -Wl,-U,_uiFlash -o $T.new || exit 1
cc -std=c99 -O2 -Wno-unused-function -DSKIN_HOST -DHOMERHIT109_OLD=1 -I viewer $SRC -lm -Wl,-U,_uiFlash -o $T.old || exit 1
$T.new "$L" "$Hm" new; rn=$?
$T.old "$L" "$Hm" new | tail -1 | sed 's/^/  красный контроль (OLD=1 по ожиданиям нового): /'; ro=${PIPESTATUS[0]}
$T.old "$L" "$Hm" old | tail -1 | sed 's/^/  прежнее по ожиданиям прежнего: /'; rp=${PIPESTATUS[0]}
rm -f $T.new $T.old
[ $rn = 0 ] && [ $ro != 0 ] && [ $rp = 0 ] && echo PASS || { echo FAIL; exit 1; }
