#!/bin/bash
# Правка 436: сборка и прогон climb436.c (повтор «лезет по стене» на данных L2). tests/g01cars/climb436.sh ИМЯ_БИНАРНИКА "V1 V2" МАШИНЫ... [-- флаги cc]
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.."; V=viewer; . tools/bots/lib.sh
N=$1; VS=$2; shift 2; CARS=(); while [ $# -gt 0 ] && [ "$1" != -- ]; do CARS+=("$1"); shift; done; [ "$1" = -- ] && shift
D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data}/l2
mkdir -p build/g436; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h -DBW_DT=bwDtNow "$@" tests/g01cars/climb436.c $BOTS_LIB -lm -o build/g436/climb_$N || exit 1
for c in "${CARS[@]}"; do for v in $VS; do BW_LEVEL=2 BW_CARNAME=$c build/g436/climb_$N $D/level.shr $D/player.shr $D/level02.msn $v ${STEER:-0}; done; done
