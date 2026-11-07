#!/bin/bash
# ТЕСТ90 П1 (g04-homer): фоновое чтение гэгов. tests/p1io/host.sh [папка набора с collect-l1.shr и l2..l7/collect-lN.shr] ; красный контроль: P1IO_RED=1
set -eu
cd "$(dirname "$0")/../.."
S="${1:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}"
D="${TMPDIR:-/tmp}/p1io.$$"; mkdir -p "$D"
RED=""; [ "${P1IO_RED:-0}" = 1 ] && RED="-DSYNCREAD_P1_OLD=1 -DP1IO_EXPECT_NEW=1"
cc -std=c11 -O1 -g -fsanitize=address,undefined -DSKIN_HOST $RED -Iviewer tests/p1io/host.c viewer/coll_stream.c viewer/gag_skin.c viewer/collect.c viewer/stateprops.c viewer/skin.c viewer/homer.c viewer/col.c viewer/car.c viewer/coins.c viewer/ticket.c viewer/props.c -lm -Wl,-U,_uiFlash -o "$D/check"
r=0
for l in 1 2 3 4 5 6 7; do p="$S/l$l/collect-l$l.shr"; [ "$l" = 1 ] && p="$S/collect-l1.shr"; "$D/check" "$p" | grep -E "L[0-9]|FAIL|PASS|ok  " || r=1; done
rm -rf "${D:?}"; exit $r
