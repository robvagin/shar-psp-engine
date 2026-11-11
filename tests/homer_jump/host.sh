#!/bin/bash
# Прыжок / двойной прыжок Гомера против записи PS2 (адрес корня 0xa245b0), хост на Маке.
#   tests/homer_jump/host.sh [level.shr] [homer.shr]
#   красный контроль: HOST_CFLAGS=-DHOMER_JUMP_PS2_OLD=1 tests/homer_jump/host.sh  (обязан упасть)
set -e
cd "$(dirname "$0")/../.."
LVL=${1:-build/level.shr}; HOM=${2:-build/homer.shr}
T=${TMPDIR:-/tmp}/homer_jump_host.$$
cc -std=c99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/homer_jump/host_jump.c viewer/homer.c viewer/watchdog.c \
   viewer/skin.c viewer/col.c viewer/car.c -lm -o "$T"
set +e
"$T" "$LVL" "$HOM"; r=$?; rm -f "$T"; exit $r
