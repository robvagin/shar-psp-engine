#!/bin/bash
# сборка тура №1002 рывок первого кадра уровня: mk.sh ИМЯ "ДОП_ФЛАГИ"  (viewer/build_ex_ИМЯ), как EBOOT тура TEST70: RELEASE+KEEP_DIAG+KEEP_HUD+KEEP_SMOKE, JITTER_*_OLD, FE_DEMO_PS2_OLD, BB_SYNC_OLD, ZF932_OLD, CARFILL930_OLD
W=$(cd "$(dirname "$0")/../.." && pwd); cd "$W"; mkdir -p build/e1002
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export PSPDEV=$W/pspdev PATH=$W/pspdev/bin:$PATH
F="-DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 -DFE_DEMO_PS2_OLD=1 -DBB_SYNC_OLD=1 -DZF932_OLD=1 -DCARFILL930_OLD=1"
( cd viewer && psp-cmake -S . -B build_ex_$1 -DSHAR_RELEASE=ON -DSHAR_RELEASE_KEEP_DIAG=ON -DSHAR_RELEASE_KEEP_HUD=ON -DSHAR_RELEASE_KEEP_SMOKE=ON -DCMAKE_C_FLAGS="$F $2" > "$W/build/e1002/cfg_$1.log" 2>&1 && cmake --build build_ex_$1 -j4 > "$W/build/e1002/b_$1.log" 2>&1 )
ls viewer/build_ex_$1/EBOOT.PBP
