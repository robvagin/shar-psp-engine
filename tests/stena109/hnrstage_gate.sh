#!/bin/bash
# Ф2-7 (109, Стена): запрет Hit & Run снимается на конце этапа. L3 m5: этап 1 (6 с) с DisableHitAndRun, потом «засчитать этап».
# Новое: stage-start и stage-end off=0; красный контроль flag hnrstage109old 1 (до конца миссии): stage-end нет.  hnrstage_gate.sh PACK_с_уровнем_3   (SHIP_LEVELS=3 ставит сам)
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SHIP_LEVELS=3 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=500 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
Rn=$(run tests/stena109/hnrstage_l3_new.txt); Ro=$(run tests/stena109/hnrstage_l3_old.txt)
cnt() { grep -c "$2" "$1/smoke.txt"; }
ns=$(cnt $Rn 'HNROFF stage-start'); ne=$(cnt $Rn 'HNROFF stage-end'); os=$(cnt $Ro 'HNROFF stage-start'); oe=$(cnt $Ro 'HNROFF stage-end')
echo "  новое: start=$ns end=$ne;  _OLD: start=$os end=$oe"
[ "$ns" = 1 ] && [ "$ne" = 1 ] && [ "$os" = 1 ] && [ "$oe" = 0 ] && echo PASS || { echo FAIL; exit 1; }
