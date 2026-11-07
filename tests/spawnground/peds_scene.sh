#!/bin/bash
# Гейт A2 Опора-93: прохожие не рождаются на неготовой сцене. Красный: -DPEDS_SCENE_OLD=1 (проверка только опоры) обязан родить хоть одного на «холодной» стороне; новое — 0.
# tests/spawnground/peds_scene.sh [LVL_ROOT] -> PASS/FAIL
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
LVL=${1:-$(ls -d $HOME/Downloads/shar-psp-integ/build/bots/missions7/*/in | tail -1)}
O=build/tmp/peds_scene; mkdir -p $O
bld() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $2 -I viewer -I tools/bots tests/spawnground/peds_scene_host.c \$BOTS_LIB -lm -o $1" || { echo "сборка: FAIL"; exit 1; }; }
bld $O/green ""; bld $O/red "-DPEDS_SCENE_OLD=1"
bad=0
for L in ${PEDS_LEVELS:-4 1}; do
  cd $LVL/L$L 2>/dev/null || continue; P=homer.shr; [ -f player.shr ] && P=player.shr
  g=$($ROOT/$O/green level.shr $P 3600 | awk -F'\t' '$1=="S"{print $2" "$3}'); r=$($ROOT/$O/red level.shr $P 3600 | awk -F'\t' '$1=="S"{print $2" "$3}')
  set -- $g; gt=${1:-0}; gb=${2:-1}; set -- $r; rt=${1:-0}; rb=${2:-0}
  ok=OK; [ "$rb" -ge 1 ] || { ok="FAIL (красный не краснеет)"; bad=1; }; [ "$gb" -eq 0 ] || { ok="FAIL (новое родило на неготовой сцене: $gb)"; bad=1; }; [ "$gt" -ge 3 ] || { ok="FAIL (новое почти не рождает: $gt — ложный ноль)"; bad=1; }
  echo "L$L: красный родил $rt, на неготовой $rb; новое родило $gt, на неготовой $gb — $ok"
  cd $ROOT
done
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
