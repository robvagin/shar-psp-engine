#!/bin/bash
# №1159: пешеходы рождаются при езде по улице (3 seed, сумма секунд). Красный контроль: прохожих нет — обязан упасть. tests/traffic/peds_drive.sh [level.shr traffic.shr]
set -u; cd "$(dirname "$0")/../.."; O=${TMPDIR:-/tmp}/peds_drive.$$
SRC="tests/traffic/peds_drive.c viewer/traffic.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c"
L=${1:-build/level.shr}; T=${2:-build/traffic.shr}
cc -std=c11 -O2 -w -DSKIN_HOST -DPEDSDRIVE_PLANT=1 -I viewer $SRC -lm -o $O.r && if $O.r "$L" "$T" > $O.r.txt; then echo "КРАСНЫЙ НЕ ПОЙМАН (прохожих нет, а прошло)"; rm -f $O.r $O.r.txt; exit 1; else echo "красный (прохожих нет): упал, как надо"; fi
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $SRC -lm -o $O.g || exit 2
$O.g "$L" "$T"; r=$?; rm -f $O.r $O.r.txt $O.g; exit $r
