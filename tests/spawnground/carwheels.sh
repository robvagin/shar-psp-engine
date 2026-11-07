#!/bin/bash
# Гейт A3 Опора-93: машина с колёсами на опоре не «висит над пустотой». Красный: -DWDG_CARWHEELS_OLD=1 (сторож смотрит только под центром кузова) обязан дать срабатывания на полосе L3; новое — 0.
# tests/spawnground/carwheels.sh [LVL_ROOT] -> PASS/FAIL
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
O=build/tmp/carwheels; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/spawnground/carwheels_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green ""; bld $O/red "-DWDG_CARWHEELS_OLD=1"
cd $LVL/L3; P=homer.shr; [ -f player.shr ] && P=player.shr
g=$($ROOT/$O/green level.shr $P | awk -F'\t' '$1=="S"{print $2" "$3" "$4}'); r=$($ROOT/$O/red level.shr $P | awk -F'\t' '$1=="S"{print $2" "$3" "$4}')
set -- $g; gn=$1; gv=$2; set -- $r; rv=$2; rw=${3:-0}; bad=0
[ "${rv:-0}" -ge 1 ] || { echo "FAIL: красный (WDG_CARWHEELS_OLD) не краснеет: $rv"; bad=1; }
[ "${gv:-1}" -eq 0 ] || { echo "FAIL: новое: срабатываний $gv"; bad=1; }
echo "L3 полоса, постановок $gn: красный nofloor car $rv (с колёсами на опоре $rw); новое $gv"
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
