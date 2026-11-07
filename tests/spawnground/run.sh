#!/bin/bash
# Гейт правки 431: tests/spawnground/run.sh [LVL_ROOT] -> PASS/FAIL. Красный: -DHOMER_SPAWNWAIT_OLD=1 -DCAR_SPAWNGROUND_OLD=1 -DWATCHDOG_OFF=1 (как до правки, без сторожа) обязан давать нарушения на тех же пробах.
# Данные: LVL_ROOT/L<n>/{level.shr,homer.shr} (по умолчанию последний набор ботов миссий основного дерева).
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
O=build/tmp/spawnground; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/spawnground/spawn_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green ""; bld $O/red "-DHOMER_SPAWNWAIT_OLD=1 -DCAR_SPAWNGROUND_OLD=1 -DWATCHDOG_OFF=1"
bad=0; RCSUM=0; N=${SPAWN_N:-40}
for L in ${SPAWN_LEVELS:-1 2 3 4 5 6 7}; do
  [ -f "$LVL/L$L/level.shr" ] || { echo "L$L: нет данных, пропуск"; continue; }
  P=homer.shr; [ -f "$LVL/L$L/player.shr" ] && P=player.shr
  g=$(cd $LVL/L$L && $ROOT/$O/green level.shr $P $N $L | awk -F'\t' '$1=="S"{print $3" "$4" "$8}')
  r=$(cd $LVL/L$L && $ROOT/$O/red level.shr $P $N $L | awk -F'\t' '$1=="S"{print $3" "$4" "$8}')
  set -- $g; gv=$1; gw=$2; gc=${3:-1}; set -- $r; rv=$1; rc=${3:-0}
  ok=OK; [ "${gv:-1}" -ne 0 ] && { ok="FAIL (новый код: нарушений $gv)"; bad=1; }; [ "${gw:-1}" -ne 0 ] && { ok="FAIL (вечное ожидание: $gw)"; bad=1; }; [ "${gc:-1}" -ne 0 ] && { ok="FAIL (машина: новый код нарушений $gc)"; bad=1; }; [ "${rv:-0}" -eq 0 ] && { ok="FAIL (красный не краснеет: герой)"; bad=1; }; RCSUM=$((RCSUM + ${rc:-0}))
  echo "L$L: герой: старый ${rv:-?} / новый ${gv:-?}, ждут в конце ${gw:-?}; машина: старый ${rc:-?} / новый ${gc:-?} — $ok"
done
[ $RCSUM -gt 0 ] || { echo "FAIL: красный не краснеет на машинах ни на одном уровне"; bad=1; }
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
