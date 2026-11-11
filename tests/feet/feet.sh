#!/bin/bash
# Модели-96, посадка стоп: сборка и запуск tests/feet/feet.c (настоящие skin.c + traffic.c, хост). Красный контроль: FEET_RED=1. Дополнительные флаги: HOST_CFLAGS
set -e
cd "$(dirname "$0")/../.."
T=${TMPDIR:-/tmp}/feet.$$
cc -std=gnu99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/feet/feet.c viewer/skin.c viewer/traffic.c viewer/car.c viewer/col.c viewer/watchdog.c -lm -o "$T" 2>&1 | grep -v "^$" | head -30
"$T" "$@"; r=$?; rm -f "$T"; exit $r
