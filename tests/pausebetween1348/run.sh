#!/bin/bash
# 1348: пауза в окне между этапами (MSN_BETWEEN, status=3) — это пауза миссии (RESTART MISSION, CANCEL MISSION), а не свободная (MISSION SELECT, LEVEL PROGRESS).
#   tests/pausebetween1348/run.sh EBOOT → прогоны новое / PAUSEBETWEEN1348_OLD=1 (сценарий tests/pauseobj1347/run.sh: этап 302 (goto) принудительно засчитывается «flag msnpasshold 8» (удерживает BETWEEN 8 с мира); от места игрока не зависит)
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
OLDFLAG=pausebetween1348old OUT=build/pausebetween1348 MSNPASS=1 MSNPASS_FLAG=msnpasshold MSNPASS_ARG=8 WAITS=${WAITS:-3} tests/pauseobj1347/run.sh $EB ${CODE:-302} > build/pausebetween1348.log 2>&1
ok=1; D=build/pausebetween1348
for v in 0 1; do f=$D/out_po1347_${CODE:-302}_$v/smoke.txt
  rows=$(grep -m1 "^PAUSEROWS" $f); st=$(grep "^MSNHUD" $f | sed -n 1p | sed 's/.*status=\([0-9]*\).*/\1/')
  echo "вариант $v (0 новое, 1 старое): status=$st  $rows"
  case $v in 0) echo "$rows" | grep -q "RESTART MISSION" && ! echo "$rows" | grep -q "MISSION SELECT" || ok=0 ;; 1) echo "$rows" | grep -q "MISSION SELECT" || ok=0 ;; esac
  [ "$st" = 3 ] || ok=0
done
[ $ok = 1 ] && echo "PAUSEBETWEEN1348 PASS" || echo "PAUSEBETWEEN1348 FAIL"; [ $ok = 1 ]
