#!/bin/bash
# Возврат к оригиналу 1284 (107b, Стена): настройки по умолчанию. cfg108_gate.sh PACK_NEW PACK_OLD  (PACK_OLD — сборка с -DSPINVIS1248_OLD=1 -DTRFDRAW108_OLD=1 -DTRFSMOOTH1092_OLD=0 -DMC_RIVAL_ROAD108_OLD=1)
# Новое: дальность машин трафика 85, сглаживание №1092 снято (smooth1092old=1), невидимые объёмы типа 76 не держат (spinvis1248old=0, прежнее правило 1248е до доказательства по исходнику), Скиннер срезает (rivalroad108old=0), уворот включён, пешеходы не помеха.
set -u; cd "$(dirname "$0")/../.."; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena108/cfg108.txt SMOKE_MAXT=120 tools/smoke.sh "$1" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
R=$(run "$1"); l=$(grep -m1 "^CFG108" "$R/smoke.txt"); echo "  новое: $l" | cut -c1-230
for k in "cardraw=85" "smooth1092old=1" "draw108old=0" "spinvis1248old=0" "rivalroad108old=0" "dodge105off=0" "dodgeexactold=0" "pedobsold=0"; do case "$l" in *"$k "*|*"$k") echo "  ok   $k";; *) echo "  FAIL $k"; fail=1;; esac; done
R=$(run "$2"); l=$(grep -m1 "^CFG108" "$R/smoke.txt"); echo "  красный (прежнее): $l" | cut -c1-230
for k in "cardraw=125" "smooth1092old=0" "spinvis1248old=1" "rivalroad108old=1"; do case "$l" in *"$k "*|*"$k") echo "  ok   КРАСНЫЙ $k";; *) echo "  FAIL $k"; fail=1;; esac; done
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
