#!/bin/bash
# Правка 434: серия заездов (flip434.c) по всему флоту; печатает переворотов до и после. Аргументы: -D... выключатели для сравнения (например -DSV_FLIPTILT_OLD=1)
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.." || exit 1
O=build/g434; mkdir -p $O
NAMES=$(python3 -c "import re;print(' '.join(sorted(set(re.findall(r'^\\s*\\{\"([a-z0-9_]+)\"',open('viewer/carparams.h').read().split('carCons[]')[1],re.M)))))")
cc -std=c99 -O2 -w -I viewer -DSV_FORCE_PLAYER=1 "$@" tests/g01cars/flip434.c viewer/col.c viewer/car.c -lm -o $O/flip_t || exit 1
$O/flip_t $NAMES
