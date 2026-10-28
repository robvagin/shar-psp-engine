#!/bin/bash
# №1034 приёмка одной командой (окно Глюки): скамейки (пропы 2,5×0,8×0,8 м) твёрдые для Гомера пешком и держат сверху; L5 проспект и L3, плюс бак L5 тип 12 и ящик L7 тип 2. accept60.sh EBOOT [SET]
#   сцена bench_L5/L3.txt: Гомер у скамейки, «старое» flag bench1034 0 (проходит насквозь), «новое» 1 (упирается; на упоре прыжок ✕ — стоит сверху: y = верх скамейки, grounded); журнал BENCH1034 (flag benchlog 1)
#   красный контроль: сценарий, где «новое» тоже flag bench1034 0 -> проверка «новое» обязана упасть.  Последняя строка PASS / FAIL
set -u
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
W=$(cd "$(dirname "$0")/../.." && pwd); cd "$W"; EB=${1:?EBOOT}; export SET=${2:-${SET:-$HOME/Downloads/shar-psp-integ/build/test72tour/PSP/GAME/SHARTEST15}}
export EMU_OWNER=${EMU_OWNER:-Glyuki} EMU_MINE=${EMU_MINE:-2} OUT=${OUT:-$W/build/b1034/acc}; mkdir -p $OUT; bad=0
for L in L5 L3; do sed 's/^flag bench1034 1$/flag bench1034 0/' tests/bench1034/bench_$L.txt > $OUT/red_$L.txt; done
for L in L5 L3; do tools/emu_run.sh tests/bench1034/bench_$L.txt $L "$EB" 300 > $OUT/$L.log 2>&1 & done; wait
for L in L5 L3; do tools/emu_run.sh $OUT/red_$L.txt r$L "$EB" 300 > $OUT/r$L.log 2>&1 & done; wait
C="python3 tests/bench1034/check.py"
for t in "L5 -158.9 29.3" "L3 200.7 -5.7"; do set -- $t
  echo "-- $1 новое (зелёный):"; $C new $OUT/out_$1/smoke.txt $2 $3 | sed 's/^/  /'; [ ${PIPESTATUS[0]} -eq 0 ] || { echo "  !! новое обязано пройти"; bad=1; }
  echo "-- $1 старое проходит насквозь (flag 0):"; $C old $OUT/out_$1/smoke.txt $2 $3 | sed 's/^/  /'; [ ${PIPESTATUS[0]} -eq 0 ] || { echo "  !! старое обязано проходить насквозь"; bad=1; }
  echo "-- $1 красный контроль (новое тоже flag 0) обязано упасть:"; $C new $OUT/out_r$1/smoke.txt $2 $3 | sed 's/^/  /'; [ ${PIPESTATUS[0]} -ne 0 ] || { echo "  !! красный не упал: проверка ничего не доказывает"; bad=1; }
done
# общее правило: баки и ящики (не скамейки) тоже твёрдые; без прыжка
for B in "L5t12 120.8 8.0" "L7t2 139.0 6.2"; do set -- $B; sed 's/^flag bench1034 1$/flag bench1034 0/' tests/bench1034/block_$1.txt > $OUT/red_$1.txt; done
for B in L5t12 L7t2; do tools/emu_run.sh tests/bench1034/block_$B.txt $B "$EB" 300 > $OUT/$B.log 2>&1 & done; wait
for B in L5t12 L7t2; do tools/emu_run.sh $OUT/red_$B.txt r$B "$EB" 300 > $OUT/r$B.log 2>&1 & done; wait
for t in "L5t12 -X 120.8 8.0 -33.3" "L7t2 -X 139.0 6.2 116.0"; do set -- $t
  echo "-- $1 бак/ящик твёрдый (зелёный):"; $C block $OUT/out_$1/smoke.txt $3 $4 $5 | sed 's/^/  /'; [ ${PIPESTATUS[0]} -eq 0 ] || { echo "  !! обязан упереться"; bad=1; }
  echo "-- $1 красный контроль (flag 0) обязан пройти насквозь:"; $C block $OUT/out_r$1/smoke.txt $3 $4 $5 | sed 's/^/  /'; [ ${PIPESTATUS[0]} -ne 0 ] || { echo "  !! красный не упал"; bad=1; }
done
if [ $bad -eq 0 ]; then echo PASS; else echo FAIL; fi
