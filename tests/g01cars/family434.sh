#!/bin/bash
# Правка 434: те же улицы уровня, тот же автопилот, разные машины (BW_CARNAME) — переворотов (наклон > 70°) за SEC игровых секунд на сид. Семейство высоких машин + опорные.
#   tests/g01cars/family434.sh [УРОВЕНЬ=4] [СЕК=600] [СИДЫ="1 2 3 4 5 6"] [машины...]   (флаги -D... для сборки — через переменную окружения CCX)
cd "$(dirname "$0")/../.."; L=${1:-4}; SECS=${2:-600}; SEEDS=${3:-"1 2 3 4 5 6"}; shift 3 2>/dev/null
CARS=${@:-"marge_v famil_v homer_v bart_v lisa_v apu_v ambul ccola dune_v garbage schoolbus"}
bash tests/g01cars/streets434.sh $L 1 "1" $CCX >/dev/null 2>&1   # сборка
O=build/g434; D=$HOME/Downloads/shar-psp/build/final-production-data; LD=$D/l$L; [ $L = 1 ] && LD=$D
SP=$(python3 -c "import struct,sys;b=open(sys.argv[1],'rb').read(80);h=struct.unpack('<14f',b[12:68]);print('%f,%f,%f,%f'%(h[6],h[8],h[7]+4,h[9]))" $LD/level.shr)
MSN=$(ls $LD/level0$L.msn 2>/dev/null | head -1)
for c in $CARS; do ( tot=0; ev=""; for s in $SEEDS; do n=$(PILOT_AT=$SP BW_LEVEL=$L BW_CARNAME=$c $O/streets_t $LD/level.shr $LD/player.shr $MSN $SECS $s 1 | grep -oE "наклон > 70°\): [0-9]+" | grep -oE "[0-9]+$"); tot=$((tot + ${n:-0})); ev="$ev ${n:-0}"; done; echo "$c: переворотов $tot за $(echo $SEEDS | wc -w | tr -d ' ') сидов по $SECS с  [по сидам:$ev ]" ) & done; wait
