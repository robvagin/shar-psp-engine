#!/bin/bash
# №1038 приёмка (Роб 26.10: «в Mission Select уровни переключаются L/R; то же в выборе машины в будке, дополнительно к прежнему»): телефонная будка L1: ► ◄ как раньше, затем R и L; flag boothlr1038 0 — R и L не работают.
# BOOTHLR1038_OLD=1 / файл boothlr1038old / flag boothlr1038 0. accept60.sh EBOOT (тур с -DZF932_OLD=1). Красный контроль: разбор журнала выключенного режима (R и L = ничего) обязан падать. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); EB=${1:-}; export OUT=${OUT:-$I/build/b1038a}; export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-2}
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test73tour/PSP/GAME/SHARTEST15}
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_a1 $OUT/out_a2 $OUT/rr_a1 $OUT/rr_a2; R=$I/tools/emu_run.sh
sed 's/^press triangle$/flag boothlr1038 0\npress triangle/' $D/booth_scenario.txt | sed 's/^flag boothlr1038 0$/flag boothlr1038 0/' > $OUT/booth_off.txt
bash $R $D/booth_scenario.txt a1 "$EB" 100 >/dev/null 2>&1 & bash $R $OUT/booth_off.txt a2 "$EB" 100 >/dev/null 2>&1 & wait
rc=0; python3 -W ignore -I $D/check.py $OUT/out_a1/smoke.txt $OUT/out_a1 | tail -12 || rc=1
python3 -W ignore -I $D/check.py $OUT/out_a1/smoke.txt $OUT/out_a1 >/dev/null 2>&1 || rc=1
echo "  красный контроль: тот же разбор на прогоне, где flag boothlr1038 0 с самого начала, обязан упасть"
if python3 -W ignore -I $D/check.py $OUT/out_a2/smoke.txt $OUT/out_a2 >/dev/null 2>&1; then echo "  КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; else echo "  ok   красный пойман"; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
