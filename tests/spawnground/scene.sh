#!/bin/bash
# Гейт правки 431 (сцена под точкой): «ещё грузится» — герой ждёт ≈1,5 с и встаёт; «не загрузилась» (state 3) — просит загрузку, ждёт не дольше 6 с, потом отпускается со строкой cellfail; не падает.
# Красный: -DHOMER_SCENEGATE_OLD=1 — герой не ждёт вовсе. tests/spawnground/scene.sh [LVL_ROOT] -> PASS/FAIL
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
O=build/tmp/scene; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/spawnground/scene_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green ""; bld $O/red "-DHOMER_SCENEGATE_OLD=1"
cd $LVL/L4; P=homer.shr; [ -f player.shr ] && P=player.shr
g=$($ROOT/$O/green level.shr $P | awk -F'\t' '$1=="S"{print $2" "$3" "$4" "$5" "$6}'); r=$($ROOT/$O/red level.shr $P | awk -F'\t' '$1=="S"{print $2}')
set -- $g; h1=$1; w=$2; h2=$3; cf=$4; fell=$5; bad=0
awk -v a=$h1 'BEGIN{exit !(a>=1.3 && a<=1.8)}' || { echo "FAIL: ждал при «грузится» $h1 с (нужно ≈1,5)"; bad=1; }
[ "$w" -ge 1 ] || { echo "FAIL: просьба загрузки не ушла"; bad=1; }
awk -v a=$h2 'BEGIN{exit !(a>=5.5 && a<=6.5)}' || { echo "FAIL: ждал при отказе $h2 с (нужно ≈6, не вечно)"; bad=1; }
[ "$cf" -ge 1 ] || { echo "FAIL: нет строки cellfail"; bad=1; }
[ "$fell" -eq 0 ] || { echo "FAIL: падение"; bad=1; }
awk -v a=${r:-9} 'BEGIN{exit !(a<0.1)}' || { echo "FAIL: красный (SCENEGATE_OLD) тоже ждёт ${r} с"; bad=1; }
echo "грузится: ждал $h1 с, просьб $w; отказ: ждал $h2 с, cellfail $cf, упал $fell; красный ждал ${r:-?} с"
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
