#!/bin/bash
# B5: хост-проверка позы водителя (tests/homer/host_driver_pose.c). Красный контроль: -DDRIVER_POSE_OLD=1 даёт «in-between 0» (всё полностью).
set -e
cd "$(dirname "$0")/../.."
T=${TMPDIR:-/tmp}/driver_pose.$$
cc -std=c99 -O2 -Wall -Wno-unused-function -DSKIN_HOST $HOST_CFLAGS -I viewer tests/homer/host_driver_pose.c viewer/homer.c viewer/watchdog.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o "$T"
"$T" "${1:-build/homer.shr}"; r=$?; rm -f "$T"; exit $r
