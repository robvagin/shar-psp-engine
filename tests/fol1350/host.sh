#!/bin/bash
# Гейт 1350 (имена): tests/fol1350/host.sh. Зелёное — машина миссии и погоня с именем cPolice живут рядом; красное (-DHNRPOLNAME1350_OLD=1) — машина миссии занимает место погони.
cd "$(dirname "$0")/../.."; V=viewer; B=${HOME}/Downloads/shar-psp/build; O=${TMPDIR:-/tmp}/fol1350h; mkdir -p $O
SRC="tests/fol1350/host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/mission/msn_file.c $V/mission/msn_ops.c $V/watchdog.c"
cc -std=c11 -O1 -w -DSKIN_HOST -I $V $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/new || exit 1
cc -std=c11 -O1 -w -DSKIN_HOST -DHNRPOLNAME1350_OLD=1 -I $V $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/old || exit 1
echo "новое:"; $O/new $B/level.shr; n=$?; echo "прежнее (красный):"; $O/old $B/level.shr; o=$?
[ $n = 0 ] && [ $o = 1 ] && echo "ГЕЙТ tests/fol1350/host: PASS" || { echo "ГЕЙТ tests/fol1350/host: FAIL (новое=$n, прежнее=$o)"; exit 1; }
