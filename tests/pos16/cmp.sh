#!/bin/bash
# Модели-96, позиции вершин s16: сборка и запуск tests/pos16/cmp.c (настоящие skin.c + traffic.c, хост). Красный контроль: POS16_RED=1 или HOST_CFLAGS=-DPOS16_TOL=0.001
set -e
cd "$(dirname "$0")/../.."
T=${TMPDIR:-/tmp}/pos16cmp.$$
cc -std=gnu99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/pos16/cmp.c viewer/skin.c viewer/traffic.c viewer/car.c viewer/col.c viewer/watchdog.c -lm -o "$T" 2>&1 | grep -v "^$" | head -30
"$T" "$@"; r=$?; rm -f "$T"; exit $r
