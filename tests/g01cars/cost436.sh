#!/bin/bash
# Правка 436: счётчики физики машин на кадр на улицах L4 (трафик + автопилот), без эмулятора. tests/g01cars/cost436.sh [УРОВЕНЬ=4] [СЕК=120] [СИД=1]
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.."; L=${1:-4}; SECS=${2:-120}; SEED=${3:-1}
D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data}; LD=$D/l$L; [ $L = 1 ] && LD=$D
SP=$(python3 -c "import struct,sys;b=open(sys.argv[1],'rb').read(80);h=struct.unpack('<14f',b[12:68]);print('%f,%f,%f,%f'%(h[6],h[8],h[7]+4,h[9]))" $LD/level.shr)
O=build/g436/cost; rm -rf $O; mkdir -p $O/cur; rsync -a --exclude 'build*' viewer $O/cur/
python3 tests/g01cars/cost_prep.py $O/cur/viewer || exit 1
V=$O/cur/viewer; . tools/bots/lib.sh
cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h -DBW_DT=bwDtNow tests/g01cars/cost.c $BOTS_LIB -lm -o $O/cost_cur 2> $O/cc.txt || { head -20 $O/cc.txt; exit 1; }
PILOT_AT=$SP BW_LEVEL=$L $O/cost_cur $LD/level.shr $LD/player.shr $(ls $LD/level0$L.msn $D/missions/level0$L.msn 2>/dev/null | head -1) $SECS $SEED 1
