#!/bin/bash
# Правка Роба 1200: jz5 (L1 F10-8 -> крыша F10-1) против 5 записей PS2. tests/jumps/jz5.sh [--scan|--gate] [level.shr] ["-DФЛАГИ"]
#   LV=путь/к/level.shr; CCF="-DФЛАГИ" — флаги сборки (например -DCAR_PITCH1200_OLD=1)
cd "$(dirname "$0")/../.."; mkdir -p build/susp
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
LVL=${LV:-$HOME/Downloads/shar-psp-integ/build/test91tour/PSP/GAME/SHARTEST15/level.shr}
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer ${CCF:-} tests/jumps/jz5.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o build/susp/jz5_x${JZ5TAG:-} || exit 2
build/susp/jz5_x${JZ5TAG:-} "$LVL" "$@"
