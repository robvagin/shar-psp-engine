#!/bin/bash
# Приёмка сторожа (CLAUDE.md п.12): каждый вид искусственно ломается (wdBreakMask), новый код чинит и пишет WATCHDOG; красный (-DWATCHDOG_OFF=1) — не чинит, строк нет.
#   tests/spawnground/watchdog.sh [LVL_ROOT] -> PASS/FAIL
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
O=build/tmp/wdg; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/spawnground/watchdog_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green "-DHOMER_VOID_RECOVER=0"; bld $O/red "-DHOMER_VOID_RECOVER=0 -DWATCHDOG_OFF=1"   # внутренний возврат Гомера выключен: проверяем именно сторожа
bad=0
cd $LVL/L1
for spec in "nofloor 1 hero-nofloor 200" "belowworld 2 hero-below 20" "stuck 4 hero-stuck 150" "nofloor 8 car-nofloor 300" "belowworld 16 car-below 20" "stuck 32 car-stuck 20"; do
  set -- $spec; k=$1; b=$2; nm=$3; fr=$4
  g=$($ROOT/$O/green level.shr homer.shr $k $b $fr | awk -F'\t' '$1=="S"{print $2" "$3" "$5}'); r=$($ROOT/$O/red level.shr homer.shr $k $b $fr | awk -F'\t' '$1=="S"{print $2}')
  set -- $g; ln=${1:-0}; ok=${2:-0}; on=${3:-0}
  res=OK; { [ "$ln" -ge 1 ] && [ "$ok" -ge 1 ]; } || { res="FAIL (нет строки/починки)"; bad=1; }; [ "${r:-0}" -ne 0 ] && { res="FAIL (красный пишет строки)"; bad=1; }
  echo "$nm: сторож строк $ln, починок ok $ok, красный строк ${r:-?} — $res"
done
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
