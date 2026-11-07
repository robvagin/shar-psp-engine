#!/bin/bash
# Правка 434: машина Мардж на L4, автопилот (streets.c), 6 сидов × 600 с (шаг 1/30): переворотов, кадров на двух колёсах с креном > 45°, крен p95 на поворотах, макс крен.
#   tests/g01cars/marge434.sh [-D... флаги сборки: -DMARGE434_OLD=1 прежнее; -DMARGE434_DY=0.2f]   (машина BW_CARNAME, по умолчанию marge_v)
cd "$(dirname "$0")/.."/..; L=4; SECS=${SECS:-600}; SEEDS=${SEEDS:-"1 2 3 4 5 6"}; CAR=${CAR:-marge_v}
bash tests/g01cars/streets434.sh $L 1 "1" "$@" >/dev/null 2>&1 || { echo "сборка не прошла"; exit 2; }
O=build/g434; D=$HOME/Downloads/shar-psp/build/final-production-data; LD=$D/l$L
SP=$(python3 -c "import struct,sys;b=open(sys.argv[1],'rb').read(80);h=struct.unpack('<14f',b[12:68]);print('%f,%f,%f,%f'%(h[6],h[8],h[7]+4,h[9]))" $LD/level.shr)
for s in $SEEDS; do ( PILOT_AT=$SP BW_LEVEL=$L BW_CARNAME=$CAR $O/streets_t $LD/level.shr $LD/player.shr $LD/level0$L.msn $SECS $s 1 > $O/m434_$s.txt ) & done; wait
fl=0; tw=0; for s in $SEEDS; do f=$(grep -oE "наклон > 70°\): [0-9]+" $O/m434_$s.txt | grep -oE "[0-9]+$"); w=$(grep -oE "крен > 45°\): [0-9]+" $O/m434_$s.txt | grep -oE "[0-9]+$"); fl=$((fl + ${f:-0})); tw=$((tw + ${w:-0})); done
read p95 mx <<< "$(python3 tests/g01cars/marge434_agg.py $O $SEEDS)"
echo "$CAR $*: переворотов $fl, кадров на двух колёсах (крен>45°) $tw, крен p95 на поворотах (медиана по сидам) ${p95}°, макс крен ${mx}°"
