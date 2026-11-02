#!/bin/bash
# Сборка полигона бонус-игры на Маке: build/bonus/bin/race_host (настоящие viewer/car.c, col.c, msncar.c, bonus.c; с 09.10 —
# props.c, stateprops.c, animobj.c: ловушки трасс). EXTRA=-DBONUS_PICKUPS_OLD=1 — красный контроль подборов
cd "$(dirname "$0")/../.."
V=viewer; mkdir -p build/bonus/bin
cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V $EXTRA tools/bonus/race_host.c $V/bonus.c $V/msncar.c \
   $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c -lm -Wl,-U,_uiFlash -o "${1:-build/bonus/bin/race_host}"
