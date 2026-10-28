#!/bin/bash
# №1060в: churn выгрузки клеток на тесной куче L4 (flag heapleft 3,2). Два маршрута (CR1/CR1b и CR0/973_L4), три режима: on (новое), churn (flag memtrim2 0 = MEMTRIM1060), burst (flag memtrim1060 0 = прежние всплески).
# Запуск: tests/memtrim2/accept60.sh [EBOOT]; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_mt2/EBOOT.PBP}; export OUT=${OUT:-$W/build/mt2/acc}; mkdir -p $OUT; rc=0
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
jobs=""; for r in 1 2; do for t in on churn burst; do s=route_$t; [ $r = 2 ] && s=route2_$t; [ "${REUSE:-0}" = 1 ] && [ -s $OUT/out_r${r}_$t/smoke.txt ] && continue; jobs="$jobs\nr${r}_$t $H/$s.txt"; done; done
printf "$jobs" | grep . | xargs -P 3 -L 1 bash -c 'bash '$H'/run.sh "$0" "$1" "'$E'" >/dev/null 2>&1'
for r in 1 2; do echo "маршрут $r:"; $W/.venv/bin/python3 -W ignore -I $H/check.py $OUT/out_r${r}_on $OUT/out_r${r}_churn $OUT/out_r${r}_burst || rc=1; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
