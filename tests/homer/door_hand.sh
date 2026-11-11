#!/bin/bash
# Роб 17.10: рука Гомера сквозь дверь при посадке — замер на Маке (tests/homer/host_door_hand.c)
set -e
cd "$(dirname "$0")/../.."
T=${TMPDIR:-/tmp}/door_hand.$$
cc -std=c99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/homer/host_door_hand.c viewer/homer.c viewer/watchdog.c viewer/skin.c \
   viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o "$T"
"$T" "${1:-build/level.shr}" "${2:-build/homer.shr}" ${3:-}; r=$?; rm -f "$T"; exit $r
