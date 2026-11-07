#!/bin/bash
# Правка 434: езда автопилота по улицам уровня на данных игры (streets.c), перевороты по семействам выключателей. Вход: tests/g01cars/streets434.sh УРОВЕНЬ [СЕК] [СИДЫ] [-D...]
#   УРОВЕНЬ — 1..7 (данные build/final-production-data/lN); итог: строки «перевороты» по сидам и сумма
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.."; V=viewer; . tools/bots/lib.sh
L=${1:-4}; SECS=${2:-300}; SEEDS=${3:-"1 2 3"}; shift 3 2>/dev/null
D=${DATA:-$HOME/Downloads/shar-psp/build/final-production-data}; LD=$D/l$L; [ $L = 1 ] && LD=$D
O=build/g434; mkdir -p $O
cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots -I tests/g01cars -include dtvar.h -DBW_DT=bwDtNow "$@" tests/g01cars/streets.c $BOTS_LIB -lm -o $O/streets_t || exit 1
MSN=$(ls $LD/level0$L.msn $D/missions/level0$L.msn 2>/dev/null | head -1)
SP=$(python3 -c "import struct,sys;b=open(sys.argv[1],'rb').read(80);h=struct.unpack('<14f',b[12:68]);print('%f,%f,%f,%f'%(h[6],h[8],h[7]+4,h[9]))" $LD/level.shr)   # спавн уровня: без bwPlace машина в мире ботов стоит
tot=0; for s in $SEEDS; do r=$(PILOT_AT=$SP BW_LEVEL=$L $O/streets_t $LD/level.shr $LD/player.shr $MSN $SECS $s 1 | grep -E "машина|перевороты|кадров"); echo "seed $s: $r" | tr '\n' ' '; echo; n=$(echo "$r" | grep -oE "наклон > 70°\): [0-9]+" | grep -oE "[0-9]+$"); tot=$((tot + ${n:-0})); done
echo "ПЕРЕВОРОТОВ ВСЕГО (наклон > 70°): $tot"
