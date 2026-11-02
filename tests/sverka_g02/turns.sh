#!/bin/bash
# №1092: бот поворотов трафика (tests/sverka_g02/turns.c). Без аргументов — уровень 5. Печатает «после» (по умолчанию) и «до» (все выключатели сверки трафика = 1).
set -u; cd "$(dirname "$0")/../.."
L=${1:-build/levels/l5/level.shr}; T=${2:-build/levels/l5/traffic.shr}; M=${3:-20}; O=${TMPDIR:-/tmp}/turns.$$
SRC="tests/sverka_g02/turns.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c"
ALL=""; for f in SPEED BRAKE JUNC CHOOSE LANECHG DODGE REJOIN HONK REMOVE SPAWN BRAKEFIX; do ALL="$ALL -DTRF_SV_${f}_OLD=1"; done
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $SRC -lm -o $O.new 2>&1 | head -5 || exit 2
cc -std=c11 -O2 -w -DSKIN_HOST $ALL -I viewer $SRC -lm -o $O.old || exit 2
echo -n "после: "; $O.new "$L" "$T" "$M"; echo -n "до:    "; $O.old "$L" "$T" "$M"; rm -f $O.new $O.old
