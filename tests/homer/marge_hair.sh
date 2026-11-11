#!/bin/bash
# Правка 1240: хост-проверка причёски Мардж в открытой и закрытой машине (tests/homer/host_marge_hair.c). Красный контроль: HOST_CFLAGS=-DMARGE_HAIR_OLD=1
set -e
cd "$(dirname "$0")/../.."
T=${TMPDIR:-/tmp}/marge_hair.$$
cc -std=c99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/homer/host_marge_hair.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o "$T"
"$T" "$@"; r=$?; rm -f "$T"; exit $r
